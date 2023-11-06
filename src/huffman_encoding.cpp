#include <algorithm>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <vector>

struct node {
   node(std::uint64_t value, std::uint64_t frequency, std::unique_ptr<node> left, std::unique_ptr<node> right) noexcept
      : value{value}, frequency{frequency}, left{std::move(left)}, right{std::move(right)}
   {}

   // this holds a value if both left and right are null
   std::uint64_t value;
   std::uint64_t frequency;
   std::unique_ptr<node> left;
   std::unique_ptr<node> right;
};

node create_tree(std::span<std::uint8_t> data) noexcept
{
   // Count data
   std::array<std::uint64_t, 256> data_counts;
   std::ranges::fill(data_counts, 0);
   for (const auto c : data) {
      data_counts[c] += 1;
      if (data_counts[c] == 0) {
         std::cerr << "std::uint64_t overflowed, exiting.\n";
         std::exit(1);
      }
   }

   // Create tree
   // We can't use priority_queue because top() only returns by const reference...
   std::vector<node> node_queue;
   for (const auto [value, count] : std::views::enumerate(data_counts)) {
      if (count > 0) {
         node_queue.emplace_back(value, count, nullptr, nullptr);
      }
   }

   // By default make_heap makes a max-heap, use > to make a min heap
   const auto comp_func = [](const auto& lhs, const auto& rhs) { return lhs.frequency > rhs.frequency; };

   std::ranges::make_heap(node_queue, comp_func);
   while (node_queue.size() > 1) {
      std::ranges::pop_heap(node_queue, comp_func);
      auto node1 = std::move(node_queue.back());
      node_queue.pop_back();

      std::ranges::pop_heap(node_queue, comp_func);
      auto node2 = std::move(node_queue.back());
      node_queue.pop_back();

      const auto new_freq = node1.frequency + node2.frequency;
      if (new_freq < node1.frequency || new_freq < node2.frequency) {
         std::cerr << "Overflow of std::uint64_t when building tree, exiting.\n";
         std::exit(1);
      }
      node_queue.emplace_back(
         0, new_freq, std::make_unique<node>(std::move(node1)), std::make_unique<node>(std::move(node2)));
      std::ranges::push_heap(node_queue, comp_func);
   }

   return std::move(node_queue.front());
}

void build_mapping(const node& n, std::map<std::uint64_t, std::string>& output, std::string so_far = "") noexcept
{
   if (n.left == nullptr && n.right == nullptr) {
      output[n.value] = so_far;
   }
   else {
      build_mapping(*n.left, output, so_far + "0");
      build_mapping(*n.right, output, so_far + "1");
   }
}

std::size_t build_raw_tree(const node& n, std::vector<std::uint16_t>& raw_tree) noexcept
{
   const auto start_loc = raw_tree.size();
   raw_tree.emplace_back(0);
   if (n.left == nullptr && n.right == nullptr) {
      raw_tree.back() |= 0b1000'0000'0000'0000;
      raw_tree.back() |= n.value;
   }
   else {
      build_raw_tree(*n.left, raw_tree);
      const auto offset = build_raw_tree(*n.right, raw_tree);
      if (offset > 0x7FFF) {
         std::cerr << "Offset too great in build_raw_tree\n";
         std::exit(1);
      }
      raw_tree[start_loc] = offset - start_loc;
   }
   return start_loc;
}

int main(int argc, const char* argv[])
{
   if (argc != 4) {
      std::cerr << "Usage:\n" << argv[0] << " input_file json_output raw_tree_output\n";
      return 2;
   }
   std::ifstream fin{argv[1], std::ios::binary};
   fin.seekg(0, std::ios::end);
   const auto size = fin.tellg();
   fin.seekg(0);
   std::vector<std::uint8_t> data;
   data.resize(size);
   fin.read(reinterpret_cast<char*>(data.data()), size);
   const auto tree = create_tree(data);
   std::map<std::uint64_t, std::string> mapping;
   build_mapping(tree, mapping);

   std::ofstream json_out{argv[2]};
   json_out << '{';
   bool comma = false;
   for (const auto& [key, value] : mapping) {
      if (comma) {
         json_out << ',';
      }
      json_out << std::quoted(std::to_string(key)) << ':' << std::quoted(value);
      comma = true;
   }
   json_out << '}';

   // This tree format is designed for speed of decoding rather than minimal size
   // Format is as follows:
   //    Each node is represented by a 16-bit integer
   //    If the top-most bit is set, the low 8-bits are the value
   //    If the top-most bit is not set, the low 15-bits are the offset to the right child
   //    The left child is always one integer ahead
   // The format is little-endian (technically platform native)
   std::ofstream tree_out{argv[3], std::ios::binary};
   std::vector<std::uint16_t> raw_tree;
   build_raw_tree(tree, raw_tree);
   tree_out.write(reinterpret_cast<const char*>(raw_tree.data()), raw_tree.size() * 2);

   // Round trip test the tree
   for (const auto& [key, value] : mapping) {
      std::size_t current_loc = 0;
      for (const auto c : value) {
         if (c == '0') {
            current_loc += 1;
         }
         else if (c == '1') {
            current_loc += raw_tree[current_loc];
         }
      }
      if ((raw_tree[current_loc] & 0xFF) != key) {
         std::cout << "Round trip for " << key << " failed; got " << (raw_tree[current_loc] & 0xFF) << " instead\n";
      }
   }
}
