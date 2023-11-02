#include <memory>
#include <type_traits>

template<typename T>
struct type_id_struct {
   static void id_func() {}
   static inline constexpr auto id = &id_func;
};

struct type_info {
private:
   using ptr = void (*)();
   ptr data_;

public:
   constexpr bool operator==(const type_info& other) const = default;
   constexpr explicit type_info(ptr data) : data_{data} {}
};

template<typename T>
inline constexpr auto type_id = type_info{type_id_struct<T>::id};

class any {
public:
   template<typename T>
   explicit any(T&& data) : data_{std::make_unique<any_impl2<std::decay_t<T>>>(std::forward<T>(data))}
   {}

   template<typename T>
   T* as_ptr() noexcept
      requires(!std::is_reference_v<T>)
   {
      if (data_ && data_->get_id() == type_id<T>) {
         return &static_cast<any_impl2<T>*>(data_.get())->value;
      }
      return nullptr;
   }

   template<typename T>
   const T* as_ptr() const noexcept
      requires(!std::is_reference_v<T>)
   {
      if (data_ && data_->get_id() == type_id<T>) {
         return &static_cast<any_impl2<T>*>(data_.get())->value;
      }
      return nullptr;
   }

   type_info type() const noexcept
   {
      if (data_) {
         return data_->get_id();
      }
      return type_id<void>;
   }

private:
   class any_impl {
   public:
      virtual type_info get_id() const = 0;
      virtual ~any_impl(){};
   };

   template<typename T>
   class any_impl2 : public any_impl {
   public:
      explicit any_impl2(const T& val) : value{val} {}
      explicit any_impl2(T&& val) : value{std::move(val)} {}
      type_info get_id() const override { return type_id<T>; }
      T value;
   };

   std::unique_ptr<any_impl> data_;
};

int main()
{
   any hi{3};
   return *hi.as_ptr<int>();
}
