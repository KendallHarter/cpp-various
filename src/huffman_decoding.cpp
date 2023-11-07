#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

template<typename T>
   requires std::is_trivially_copyable_v<T>
std::vector<T> read_file(const std::string& read_loc)
{
   std::ifstream fin{read_loc.c_str(), std::ios::binary};
   fin.seekg(0, std::ios::end);
   const auto size = fin.tellg();
   fin.seekg(0);
   std::vector<T> to_ret;
   to_ret.resize(size);
   fin.read(reinterpret_cast<char*>(to_ret.data()), size);
   return to_ret;
}

// Should really make this an iterator but lazy
struct bit_reader {
   bool get_bit()
   {
      if (num_bits == 0) {
         if (current_loc >= data->size()) {
            std::cerr << "Ran out of data to read\n";
            std::exit(2);
         }
         current = (*data)[current_loc];
         current_loc += 1;
         num_bits = 8;
      }
      const auto to_output = current & 0b1000'0000;
      current <<= 1;
      num_bits -= 1;
      return to_output;
   }

   std::uint8_t current;
   int num_bits;
   std::size_t current_loc;
   const std::vector<std::uint8_t>* data;
};

int main(int argc, const char* argv[])
{
   if (argc != 5) {
      std::cerr << "Usage:\n" << argv[0] << " huffman_tree to_decompress output_size output_file\n";
      return 1;
   }
   const auto output_size = std::atoi(argv[3]);
   if (output_size <= 0) {
      std::cerr << "Invalid output_size of " << output_size << '\n';
      return 1;
   }
   const auto tree = read_file<std::uint16_t>(argv[1]);
   const auto to_decompress = read_file<std::uint8_t>(argv[2]);

   std::ofstream fout{argv[4], std::ios::binary};
   bit_reader reader{0, 0, 0, &to_decompress};
   for (int i = 0; i < output_size; ++i) {
      std::size_t decode_loc = 0;
      while (!(tree[decode_loc] & 0b1000'0000'0000'0000)) {
         if (decode_loc >= tree.size()) {
            std::cerr << "Trying to access tree out of bounds\n";
            return 2;
         }
         if (reader.get_bit()) {
            decode_loc += tree[decode_loc];
         }
         else {
            decode_loc += 1;
         }
      }
      const std::uint8_t to_write = tree[decode_loc] & 0x7FFF;
      fout.write(reinterpret_cast<const char*>(&to_write), 1);
   }
}
