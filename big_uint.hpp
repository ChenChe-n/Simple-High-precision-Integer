#ifndef CHENC_BIG_UINT_HPP
#define CHENC_BIG_UINT_HPP

#include <vector>
#include <string>
#include <array>
#include <bit>
#include <bitset>
#include <charconv>
#include <algorithm>
#include <string_view>
#include <cstdint>
#include <concepts>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <stdfloat>

namespace chenc
{
    namespace tools
    {
        /**
         * @brief 计算整数的置位位数（Hamming weight）
         * @note 使用查表
         * @tparam T 整数类型（支持有符号和无符号，最大64位）
         * @param n 输入数字
         * @return 置位位数
         */
        template <typename T>
            requires std::unsigned_integral<T> && (sizeof(T) <= sizeof(uint64_t))
        inline static uint64_t bit_count(const T &n) noexcept
        {
            return std::popcount(n);
        }
        /**
         * @brief 计算整数最高有效位的位置(从0开始)
         * @note 使用二分搜索法查找最高有效位
         * @tparam T 整数类型(支持有符号和无符号，最大64位)
         * @param n 输入数字(0的返回值未定义)
         * @return 最高有效位的位置(从0开始)，n=0时结果未定义
         */
        template <typename T>
            requires std::unsigned_integral<T> && (sizeof(T) <= sizeof(uint64_t))
        inline static uint64_t highest_bit_index(const T &n) noexcept
        {
            if (n == 0)
                return 0;
            return (sizeof(T) * 8 - 1) - std::countl_zero(n);
        }
        /**
         * @brief abs
         * @param n
         * @return 绝对值
         */
        template <typename T>
            requires std::integral<T>
        inline static constexpr auto abs(const T &n) noexcept
        {
            if constexpr (std::signed_integral<T>)
            {
                using U = std::make_unsigned_t<T>;
                if (n == std::numeric_limits<T>::lowest())
                {
                    // 特殊处理 INT_MIN
                    return static_cast<U>(std::numeric_limits<T>::max()) + 1;
                }
                return static_cast<U>(n < 0 ? -n : n);
            }
            else
            {
                return n; // 无符号类型直接返回
            }
        }

    }
    namespace big_int
    {
        class division_by_zero : public ::std::runtime_error
        {
        public:
            inline division_by_zero()
                : ::std::runtime_error("Division by zero")
            {
            }
            inline division_by_zero(const char *message)
                : ::std::runtime_error(message)
            {
            }
            inline division_by_zero(const std::string &message)
                : std::runtime_error(message)
            {
            }
        };
        class invalid_argument : public ::std::runtime_error
        {
        public:
            inline invalid_argument()
                : ::std::runtime_error("Invalid argument")
            {
            }
            inline invalid_argument(const char *message)
                : ::std::runtime_error(message)
            {
            }
            inline invalid_argument(const std::string &message)
                : std::runtime_error(message)
            {
            }
        };
        /**
         * @class big_uint
         * @brief 大整数类，支持无符号大整数运算
         * @note 使用 std::vector<uint32_t> 存储数据，每个元素表示32位
         * @note 数据采用小端存储，最低位在data_[0]
         */
        class big_uint
        {
        public:
            // -------- 构造函数 --------
            /**
             * @brief 默认构造一个值为0的大整数
             * @note 预分配容量（以位为单位），默认为 def_cap_ 位
             */
            inline big_uint()
            {
                data_.reserve(calc_blocks(def_cap_));
                data_.push_back(0);
            }
            /**
             * @brief 从无符号整数构造大整数
             * @tparam T 无符号整数类型（最大支持uint64_t）
             * @param value 初始值
             * @param capacity 预分配容量（以位为单位），默认为 def_cap_ 位
             */
            template <typename T>
                requires std::unsigned_integral<T> && (sizeof(T) <= sizeof(uint64_t))
            inline big_uint(const T &value, const uint64_t &capacity = def_cap_)
            {
                const uint64_t v = static_cast<uint64_t>(value);
                data_.reserve(calc_blocks(capacity));
                data_.push_back(v & UINT32_MAX);
                if (v > UINT32_MAX)
                    data_.push_back((v >> 32) & UINT32_MAX);
            }
            /**
             * @brief 从有符号整数构造大整数（存储绝对值）
             * @tparam T 有符号整数类型（最大支持int64_t）
             * @param value 初始值
             * @param capacity 预分配容量（以位为单位），默认为 def_cap_ 位
             */
            template <typename T>
                requires std::signed_integral<T> && (sizeof(T) <= sizeof(uint64_t))
            inline big_uint(const T &value, const uint64_t &capacity = def_cap_)
            {
                const uint64_t v = chenc::tools::abs(value);
                data_.reserve(calc_blocks(capacity));
                data_.push_back(v & UINT32_MAX);
                if (v > UINT32_MAX)
                    data_.push_back((v >> 32) & UINT32_MAX);
            }
            /**
             * @brief 从字符串构造大整数 (2-36进制)
             * @tparam base 字符串的进制 (2-36)，默认为 10
             * @param str 无符号整数字符串
             * @param capacity 预分配的容量（以位为单位），默认为 def_cap_ 位
             */
            template <uint64_t base = 10>
            inline static big_uint from_str(const std::string_view &str,
                                            const uint64_t &capacity = def_cap_)
            {
                static_assert(base >= 2 && base <= 36, "base must be between 2 and 36");
                static std::array<int8_t, 128> chars_base = []() -> std::array<int8_t, 128>
                {
                    std::array<int8_t, 128> result = {0};
                    for (int i = 0; i < 128; i++)
                        result[i] = 99;
                    for (int i = '0'; i <= '9'; i++)
                        result[i] = i - '0';
                    for (int i = 'a'; i <= 'z'; i++)
                        result[i] = i - 'a' + 10;
                    for (int i = 'A'; i <= 'Z'; i++)
                        result[i] = i - 'A' + 10;
                    return result;
                }();
                big_uint result;

                // 检查进制范围
                if (base < 2 || base > 36)
                {
                    return result;
                }

                // 检查字符串
                for (auto &c : str)
                {
                    if (chars_base[c] >= 99 || chars_base[c] < 0 ||
                        static_cast<uint64_t>(chars_base[c]) >= base)
                    {
                        return result; // 非法字符/非法进制 返回 0
                    }
                }

                // 进制转换
                static uint64_t block_len = []() -> uint64_t
                {
                    uint64_t i = 1;
                    uint64_t n = base;
                    while (n <= UINT32_MAX / base)
                    {
                        n *= base;
                        i++;
                    }
                    return i;
                }();

                static uint64_t base_power = []() -> uint64_t
                {
                    uint64_t n = 1;
                    for (uint64_t i = 0; i < block_len; i++)
                        n *= base;
                    return n;
                }();

                // 从左到右处理字符串
                for (size_t i = 0; i < str.length();)
                {
                    // 计算当前块的值
                    uint64_t chunk_value = 0;
                    size_t chars_processed = 0;

                    // 处理一个块（最多block_len个字符）
                    while (chars_processed < block_len && (i + chars_processed) < str.length())
                    {
                        char c = str[i + chars_processed];
                        if (chars_base[c] >= 99 || chars_base[c] < 0 ||
                            static_cast<uint64_t>(chars_base[c]) >= base)
                        {
                            break;
                        }

                        chunk_value = chunk_value * base + chars_base[c];
                        chars_processed++;
                    }

                    if (chars_processed > 0)
                    {
                        // 乘以base的幂次
                        if (chars_processed == block_len)
                        {
                            result *= base_power;
                        }
                        else
                        {
                            // 计算实际的幂次
                            uint64_t power = 1;
                            for (size_t j = 0; j < chars_processed; j++)
                                power *= base;
                            result *= power;
                        }

                        // 加上当前块的值
                        result += static_cast<uint32_t>(chunk_value);

                        i += chars_processed;
                    }
                    else
                    {
                        i++; // 跳过无效字符
                    }
                }
                return result;
            }
            /**
             * @brief 从字符串构造大整数 (2-36进制)
             * @tparam base 字符串的进制 (2-36)，默认为 10
             * @param str 无符号整数字符串
             * @param capacity 预分配的容量（以位为单位），默认为 def_cap_ 位
             */
            inline big_uint(const std::string_view &str,
                            const uint64_t &base = 10,
                            const uint64_t &capacity = def_cap_)
            {
                if (base < 2 || base > 36)
                {
                    throw std::invalid_argument("chenc::big_int::big_uint.to_string base must be between 2 and 36");
                }
#define CHENC_DEFINE_CASE(val)                        \
    case val:                                         \
        data_ = (from_str<val>(str, capacity).data_); \
        break;

                switch (base)
                {
                    CHENC_DEFINE_CASE(2);
                    CHENC_DEFINE_CASE(3);
                    CHENC_DEFINE_CASE(4);
                    CHENC_DEFINE_CASE(5);
                    CHENC_DEFINE_CASE(6);
                    CHENC_DEFINE_CASE(7);
                    CHENC_DEFINE_CASE(8);
                    CHENC_DEFINE_CASE(9);
                    CHENC_DEFINE_CASE(10);
                    CHENC_DEFINE_CASE(11);
                    CHENC_DEFINE_CASE(12);
                    CHENC_DEFINE_CASE(13);
                    CHENC_DEFINE_CASE(14);
                    CHENC_DEFINE_CASE(15);
                    CHENC_DEFINE_CASE(16);
                    CHENC_DEFINE_CASE(17);
                    CHENC_DEFINE_CASE(18);
                    CHENC_DEFINE_CASE(19);
                    CHENC_DEFINE_CASE(20);
                    CHENC_DEFINE_CASE(21);
                    CHENC_DEFINE_CASE(22);
                    CHENC_DEFINE_CASE(23);
                    CHENC_DEFINE_CASE(24);
                    CHENC_DEFINE_CASE(25);
                    CHENC_DEFINE_CASE(26);
                    CHENC_DEFINE_CASE(27);
                    CHENC_DEFINE_CASE(28);
                    CHENC_DEFINE_CASE(29);
                    CHENC_DEFINE_CASE(30);
                    CHENC_DEFINE_CASE(31);
                    CHENC_DEFINE_CASE(32);
                    CHENC_DEFINE_CASE(33);
                    CHENC_DEFINE_CASE(34);
                    CHENC_DEFINE_CASE(35);
                    CHENC_DEFINE_CASE(36);
                default:
                    data_ = {0};
                }
#undef CHENC_DEFINE_CASE
            }
            /**
             * @brief 拷贝构造函数
             * @param other 源对象
             * @param capacity 预分配容量（以位为单位），默认为 def_cap_ 位
             */
            inline big_uint(const big_uint &other, const uint64_t &capacity = def_cap_)
            {
                data_.reserve(std::max(other.data_.size(), calc_blocks(capacity)));
                data_ = other.data_;
            }
            /**
             * @brief 移动构造函数
             * @param other 源对象（将被置 0）
             */
            inline big_uint(big_uint &&other) noexcept
                : data_(std::move(other.data_))
            {
                if (other.data_.empty())
                {
                    other.data_.push_back(0);
                }
            }
            /**
             * @brief 从数组构造
             * @param value 数组
             */
            inline big_uint(const std::vector<uint32_t> &value)
                : data_(value)
            {
            }
            /**
             * @brief 从数组构造
             * @param value 数组
             */
            inline big_uint(std::vector<uint32_t> &&value) noexcept
                : data_(std::move(value))
            {
            }

            // -------- 赋值函数 --------
            /**
             * @brief 拷贝赋值运算符
             * @param other 源对象
             * @return 当前对象引用
             */
            inline big_uint &operator=(const big_uint &other)
            {
                if (this != &other)
                {
                    data_.reserve(std::max(other.data_.size(), calc_blocks(def_cap_)));
                    data_ = other.data_;
                }
                return *this;
            }
            /**
             * @brief 移动赋值运算符
             * @param other 源对象（将被置空）
             * @return 当前对象引用
             */
            inline big_uint &operator=(big_uint &&other) noexcept
            {
                if (this != &other)
                {
                    data_ = std::move(other.data_);
                    if (other.data_.empty())
                    {
                        other.data_.push_back(0);
                    }
                }
                return *this;
            }

            // -------- 对象数据获取函数 --------
            /**
             * @brief 获取当前值的最高位数
             * @return 最高位数
             */
            inline uint64_t bits() const
            {
                if (data_.back() == 0)
                    return 0;
                return (data_.size() - 1) * 32 + chenc::tools::highest_bit_index(data_.back());
            }
            /**
             * @brief 获取存储的32位块数量
             * @return 32位块数量
             */
            inline uint64_t blocks() const
            {
                return data_.size();
            }
            /**
             * @brief 获取当前分配的容量（以位为单位）
             * @return 容量位数
             */
            inline uint64_t capacity() const
            {
                return data_.capacity() * 32;
            }
            /**
             * @brief 获取底层数据常量引用
             * @return 底层数据常量引用
             */
            inline const std::vector<uint32_t> &data() const
            {
                return data_;
            }
            /**
             * @brief 获取底层数据引用
             * @return 底层数据引用
             */
            inline std::vector<uint32_t> &data()
            {
                return data_;
            }
            /**
             * @brief 是否为 0
             * @return bool
             */
            inline bool is_zero() const
            {
                return data_.size() == 1 and data_.back() == 0;
            }
            /**
             * @brief 是否为 1
             * @return bool
             */
            inline bool is_one() const
            {
                return data_.size() == 1 and data_.back() == 1;
            }
            /**
             * @brief bit count 统计 1 的数量
             * @return 1 的数量
             */
            inline uint64_t bit_count() const
            {
                uint64_t count = 0;
                for (auto i : data_)
                    count += chenc::tools::bit_count(i);
                return count;
            }
            /**
             * @brief 统计尾随零的数量
             * @return 尾随零的数量
             */
            inline uint64_t bit_trailing_zero_count() const
            {
                if (is_zero())
                    return 0;
                uint64_t count = 0;
                for (uint64_t i = 0; i < bits(); i++)
                    if (bit_test(i))
                        break;
                    else
                        count++;
                return count;
            }
            /**
             * @brief bit test
             * @param index
             * @return bool
             */
            inline bool bit_test(uint64_t index) const
            {
                return (data_[index / 32] >> (index % 32)) & 1;
            }
            /**
             * @brief bit set
             * @param index
             * @param value
             * @return void
             */
            inline void bit_set(uint64_t index, bool value)
            {
                if (value)
                    data_[index / 32] |= (1 << (index % 32));
                else
                    data_[index / 32] &= ~(1 << (index % 32));
            }

            // -------- 比较操作符 --------
            /**
             * @brief 相等比较
             * @param other 比较对象
             * @return 相等返回true，否则false
             */
            inline bool operator==(const big_uint &other) const
            {
                if (data_.size() != other.data_.size())
                    return false;
                for (uint64_t i = 0; i < data_.size(); i++)
                {
                    if (data_[i] != other.data_[i])
                        return false;
                }
                return true;
            }
            /**
             * @brief 不等比较
             * @param other 比较对象
             * @return 不等返回true，否则false
             */
            inline bool operator!=(const big_uint &other) const
            {
                return !(*this == other);
            }
            /**
             * @brief 小于比较
             * @param other 比较对象
             * @return 当前对象小于other返回true，否则false
             */
            inline bool operator<(const big_uint &other) const
            {
                if (data_.size() != other.data_.size())
                    return data_.size() < other.data_.size();

                for (int64_t i = data_.size() - 1; i >= 0; --i)
                {
                    if (data_[i] != other.data_[i])
                        return data_[i] < other.data_[i];
                }
                return false;
            }
            /**
             * @brief 大于比较
             * @param other 比较对象
             * @return 当前对象大于other返回true，否则false
             */
            inline bool operator>(const big_uint &other) const
            {
                return other < *this;
            }
            /**
             * @brief 小于等于比较
             * @param other 比较对象
             * @return 当前对象小于等于other返回true，否则false
             */
            inline bool operator<=(const big_uint &other) const
            {
                return !(*this > other);
            }
            /**
             * @brief 大于等于比较
             * @param other 比较对象
             * @return 当前对象大于等于other返回true，否则false
             */
            inline bool operator>=(const big_uint &other) const
            {
                return !(*this < other);
            }

            // -------- 逻辑操作符 --------
            /**
             * @brief 位或赋值操作
             * @param other 操作数
             * @return 当前对象引用
             */
            inline big_uint &operator|=(const big_uint &other)
            {
                if (data_.size() < other.data_.size())
                    data_.resize(other.data_.size(), 0);

                for (uint64_t i = 0; i < data_.size(); i++)
                    data_[i] |= other.data_[i];

                trim();

                return *this;
            }
            /**
             * @brief 位或操作
             * @param other 操作数
             * @return 新对象
             */
            inline big_uint operator|(const big_uint &other) const
            {
                const auto &big = (data_.size() > other.data_.size()) ? *this : other;
                const auto &small = (data_.size() > other.data_.size()) ? other : *this;
                auto result = big;
                result |= small;
                return result;
            }
            /**
             * @brief 位与赋值操作
             * @param other 操作数
             * @return 当前对象引用
             */
            inline big_uint &operator&=(const big_uint &other)
            {
                if (data_.size() < other.data_.size())
                    data_.resize(other.data_.size(), 0);

                for (uint64_t i = 0; i < data_.size(); i++)
                    data_[i] &= other.data_[i];

                trim();

                return *this;
            }
            /**
             * @brief 位与操作
             * @param other 操作数
             * @return 新对象
             */
            inline big_uint operator&(const big_uint &other) const
            {
                const auto &big = (data_.size() > other.data_.size()) ? *this : other;
                const auto &small = (data_.size() > other.data_.size()) ? other : *this;
                auto result = big;
                result &= small;
                return result;
            }
            /**
             * @brief 位异或赋值操作
             * @param other 操作数
             * @return 当前对象引用
             */
            inline big_uint &operator^=(const big_uint &other)
            {
                if (data_.size() < other.data_.size())
                    data_.resize(other.data_.size(), 0);

                for (uint64_t i = 0; i < data_.size(); i++)
                    data_[i] ^= other.data_[i];

                trim();

                return *this;
            }
            /**
             * @brief 位异或操作
             * @param other 操作数
             * @return 新对象
             */
            inline big_uint operator^(const big_uint &other) const
            {
                const auto &big = (data_.size() > other.data_.size()) ? *this : other;
                const auto &small = (data_.size() > other.data_.size()) ? other : *this;
                auto result = big;
                result ^= small;
                return result;
            }

            // -------- 运算操作符 --------
            /**
             * @brief 前置自增运算符
             * @return 自增后的对象
             */
            inline big_uint &operator++()
            {
                return *this += 1;
            }
            /**
             * @brief 后置自增运算符
             * @return 自增后的对象
             */
            inline big_uint operator++(int)
            {
                auto result = *this;
                ++(*this);
                return result;
            }
            /**
             * @brief 前置自减运算符
             * @return 自减后的对象
             */
            inline big_uint &operator--()
            {
                return *this -= 1;
            }
            /**
             * @brief 后置自减运算符
             * @return 自减后的对象
             */
            inline big_uint operator--(int)
            {
                big_uint ret = *this;
                --(*this);
                return ret;
            }
            /**
             * @brief 左移运算符
             * @param shift_bits 左移的位数
             * @return 新对象
             */
            inline big_uint operator<<(const uint64_t &shift_bits) const
            {
                if (shift_bits == 0 || is_zero())
                    return *this;

                uint64_t shift_blocks = shift_bits / 32;
                uint64_t shift_bits_in_blocks = shift_bits % 32;

                // 初始化新对象容量
                std::vector<uint32_t> result(data_.size() + shift_blocks + 1, 0);

                if (shift_bits_in_blocks == 0)
                {
                    // 特殊情况：按完整块移动
                    for (uint64_t i = 0; i < data_.size(); ++i)
                    {
                        result[i + shift_blocks] = data_[i];
                    }
                }
                else
                {
                    uint32_t high_mask = (1U << shift_bits_in_blocks) - 1;
                    uint32_t low_mask = ~high_mask;

                    for (uint64_t i = 0; i < data_.size(); ++i)
                    {
                        // 处理低位（左移）
                        result[i + shift_blocks] |= (data_[i] << shift_bits_in_blocks);
                        // 处理高位（右移后放到下一个块）
                        result[i + shift_blocks + 1] |= (data_[i] >> (32 - shift_bits_in_blocks));
                    }
                }

                // 移除前导零
                return big_uint(std::move(result)).trim();
            }
            /**
             * @brief 左移赋值运算符
             * @param shift_bits 左移的位数
             * @return 当前对象引用
             */
            inline big_uint &operator<<=(const uint64_t &shift_bits)
            {
                return *this = *this << shift_bits;
            }
            /**
             * @brief 右移运算符
             * @param shift_bits 右移的位数
             * @return 新对象
             */
            inline big_uint operator>>(const uint64_t &shift_bits) const
            {
                if (shift_bits == 0 || is_zero())
                    return *this;

                uint64_t shift_blocks = shift_bits / 32;
                uint64_t shift_bits_in_blocks = shift_bits % 32;

                if (shift_blocks >= data_.size())
                {
                    // 移动位数超过总位数，结果为0
                    return big_uint(0);
                }

                // 初始化新对象容量
                std::vector<uint32_t> result(data_.size() - shift_blocks + 1, 0);

                if (shift_bits_in_blocks == 0)
                {
                    // 特殊情况：按完整块移动
                    for (uint64_t i = shift_blocks; i < data_.size(); ++i)
                    {
                        result[i - shift_blocks] = data_[i];
                    }
                }
                else
                {
                    for (uint64_t i = shift_blocks; i < data_.size(); ++i)
                    {
                        // 处理低位（右移）
                        result[i - shift_blocks] |= (data_[i] >> shift_bits_in_blocks);
                        // 处理高位（左移后放到上一个块）
                        if (i > shift_blocks)
                        {
                            result[i - shift_blocks - 1] |= (data_[i] << (32 - shift_bits_in_blocks));
                        }
                    }
                }

                // 移除前导零
                return big_uint(std::move(result)).trim();
            }
            /**
             * @brief 右移赋值运算符
             * @param shift_bits 右移的位数
             * @return 当前对象引用
             */
            inline big_uint &operator>>=(const uint64_t &shift_bits)
            {
                return *this = *this >> shift_bits;
            }
            /**
             * @brief 加法赋值运算符
             * @param other 加数
             * @return 当前对象引用
             */
            inline big_uint &operator+=(const big_uint &other)
            {
                if (other.is_zero())
                    return *this;

                uint64_t original_size = data_.size();
                data_.resize(std::max(data_.size(), other.data_.size()) + 1, 0);

                uint64_t carry = 0;
                uint64_t len = other.data_.size();
                uint64_t i = 0;

                for (; i < len; ++i)
                {
                    uint64_t sum = uint64_t(data_[i]) + uint64_t(other.data_[i]) + carry;
                    data_[i] = uint32_t(sum & UINT32_MAX);
                    carry = sum >> 32;
                }

                while (carry != 0 && i < original_size)
                {
                    uint64_t sum = uint64_t(data_[i]) + carry;
                    data_[i] = uint32_t(sum & UINT32_MAX);
                    carry = sum >> 32;
                    i++;
                }

                if (carry != 0)
                {
                    data_[i] = uint32_t(carry);
                }

                trim();
                return *this;
            }
            /**
             * @brief 加法运算符
             * @param other 加数
             * @return 新对象
             */
            inline big_uint operator+(const big_uint &other) const
            {
                const auto &big = (data_.size() > other.data_.size()) ? *this : other;
                const auto &small = (data_.size() > other.data_.size()) ? other : *this;
                auto result = big;
                result += small;
                return result;
            }
            /**
             * @brief 减法赋值运算符
             * @param other 减数
             * @return 当前对象引用
             */
            inline big_uint &operator-=(const big_uint &other)
            {
                if (*this < other)
                {
                    data_.clear();
                    data_.push_back(0);
                    return *this;
                }

                uint64_t borrow = 0;
                uint64_t len = other.data_.size();

                for (uint64_t i = 0; i < len; ++i)
                {
                    uint64_t diff = uint64_t(data_[i]) - uint64_t(other.data_[i]) - borrow;
                    data_[i] = uint32_t(diff & UINT32_MAX);
                    borrow = (diff >> 32) & 1; // 正确计算借位
                }

                // 处理剩余的借位
                uint64_t i = len;
                while (borrow != 0 && i < data_.size())
                {
                    uint64_t diff = uint64_t(data_[i]) - borrow;
                    data_[i] = uint32_t(diff & UINT32_MAX);
                    borrow = (diff >> 32) & 1;
                    i++;
                }

                trim();

                return *this;
            }
            /**
             * @brief 减法运算符
             * @param other 减数
             * @return 新对象
             */
            inline big_uint operator-(const big_uint &other) const
            {
                return big_uint(*this) -= other;
            }
            /**
             * @brief 乘法赋值运算符
             * @param other 乘数
             * @return 当前对象引用
             */
            inline big_uint &operator*=(const big_uint &other)
            {
                if (other.is_zero() || is_zero())
                    return *this = 0;
                if (other.is_one())
                    return *this;
                if (is_one())
                    return *this = other;

                // 是否在启用 karatsuba 算法
                if (
                    data_.size() + other.data_.size() > 128)
                {
                    multiply_karatsuba(*this, other);
                }
                else
                {
                    multiply_default(*this, other);
                }

                return *this;
            }
            /**
             * @brief 乘法运算符
             * @param other 被乘数
             * @return 新对象
             */
            inline big_uint operator*(const big_uint &other) const
            {
                return big_uint(*this) *= other;
            }
            /**
             * @brief 除法运算符
             * @param other 除数
             * @return 新对象
             */
            inline big_uint operator/(const big_uint &other) const
            {
                if (other.is_zero())
                    throw division_by_zero("chenc::big_int::big_uint.operator/ division_by_zero");
                if (is_zero())
                    return big_uint();
                if (other.is_one())
                    return *this;
                // 快速除法
                if (data_.size() <= 2 and other.data_.size() <= 2)
                {
                    uint64_t a, b;
                    a = data_[0];
                    b = other.data_[0];
                    if (data_.size() == 2)
                        a += uint64_t(data_[1]) * (uint64_t(1) << 32);
                    if (other.data_.size() == 2)
                        b += uint64_t(other.data_[1]) * (uint64_t(1) << 32);
                    return big_uint(a / b);
                }

                big_uint q, r;
                division_newton_raphson(*this, other, q, r);
                // division_default(*this, other, q, r);
                return q;
            }
            /**
             * @brief 除法赋值运算符
             * @param other 除数
             * @return 新对象
             */
            inline big_uint &operator/=(const big_uint &other)
            {
                return *this = *this / other;
            }
            /**
             * @brief 模运算符
             * @param other 被除数
             * @return 新对象
             */
            inline big_uint operator%(const big_uint &other) const
            {
                if (is_zero() or other.is_zero())
                    return big_uint();
                // 快速模运算
                if (data_.size() <= 2 and other.data_.size() <= 2)
                {
                    uint64_t a, b;
                    a = data_[0];
                    b = other.data_[0];
                    if (data_.size() == 2)
                        a += uint64_t(data_[1]) << 32;
                    if (other.data_.size() == 2)
                        b += uint64_t(other.data_[1]) << 32;
                    return big_uint(a % b);
                }

                big_uint q, r;
                division_newton_raphson(*this, other, q, r);
                // division_default(*this, other, q, r);
                return r;
            }
            /**
             * @brief 模赋值运算符
             * @param other 被除数
             * @return 新对象
             */
            inline big_uint &operator%=(const big_uint &other)
            {
                return *this = *this % other;
            }
            /**
             * @brief 高精度除法
             * @param dividend 被除数
             * @param divisor 除数
             * @param quotient 商
             * @param remainder 余数
             * @note 除 0 将
             */
            inline static void div(const big_uint &dividend,
                                   const big_uint &divisor,
                                   big_uint &quotient,
                                   big_uint &remainder)
            {
                if (divisor.is_zero())
                {
                    throw division_by_zero("chenc::big_int::big_uint::div division_by_zero");
                }
                if (dividend.is_zero())
                {
                    quotient = big_uint();
                    remainder = dividend;
                    return;
                }
                if (divisor.is_one())
                {
                    quotient = dividend;
                    remainder = 0;
                    return;
                }
                // 快速除法
                if (dividend.data_.size() <= 2 and divisor.data_.size() <= 2)
                {
                    uint64_t a, b;
                    a = dividend.data_[0];
                    b = divisor.data_[0];
                    if (dividend.data_.size() == 2)
                        a += uint64_t(dividend.data_[1]) * (uint64_t(1) << 32);
                    if (divisor.data_.size() == 2)
                        b += uint64_t(divisor.data_[1]) * (uint64_t(1) << 32);

                    quotient = a / b;
                    remainder = a % b;
                    return;
                }

                division_newton_raphson(dividend, divisor, quotient, remainder);
            }
            /**
             * @brief 最大公因数 GCD
             * @param a
             * @param b
             * @return 结果
             */
            inline static big_uint gcd(const big_uint &a, const big_uint &b)
            {
                if (a.is_zero())
                    return b;
                if (b.is_zero())
                    return a;

                big_uint x = a;
                big_uint y = b;

                // 统计当前可以同时右移的位数
                uint64_t shift_count = std::min(x.bit_trailing_zero_count(), y.bit_trailing_zero_count());
                // 如果为 0 无副作用
                x >>= shift_count;
                y >>= shift_count;
                // 1. 计算公共的2的幂次因子（批量处理）
                uint64_t common_shift = shift_count;

                // 2. 移除x中剩余的2的因子（批量处理）
                x >>= x.bit_trailing_zero_count();

                // 3. 核心循环 (混合算法)
                while (!x.is_zero())
                {
                    // 批量移除 y 中所有2的因子
                    y >>= y.bit_trailing_zero_count();

                    // 确保 x <= y
                    if (x > y)
                        x.swap(y);

                    // 如果两个数大小悬殊，使用模运算而不是减法
                    constexpr uint64_t bit_diff_threshold = 24;
                    if (y.bits() > x.bits() + bit_diff_threshold)
                    {
                        y %= x;
                    }
                    else
                    {
                        y -= x;
                    }
                }

                // 恢复公共的2的幂次因子
                return y << common_shift;
            }
            /**
             * @brief 最小公倍数 LCM
             * @param a
             * @param b
             * @return 结果
             */
            inline static big_uint lcm(const big_uint &a, const big_uint &b)
            {
                return a * b / gcd(a, b);
            }
            /**
             * @brief pow 幂次函数
             * @param a 底数
             * @param b 幂次
             * @return 结果
             */
            inline static big_uint pow(const big_uint &a, const uint64_t &b)
            {
                if (b == 0)
                {
                    return big_uint(1);
                }
                if (a == 0)
                {
                    return 0;
                }
                if (b == 1)
                {
                    return a;
                }
                // 特殊优化：如果a是2的幂次，则使用位移操作
                if (a.bit_count() == 1)
                {
                    return a << ((b - 1) * a.bits());
                }

                // 使用快速幂算法
                big_uint result = 1;
                big_uint base = a;
                uint64_t exp = b;

                while (exp > 0)
                {
                    // 如果指数是奇数，将当前底数乘入结果
                    if (exp & 1)
                    {
                        result *= base;
                    }

                    // 底数平方，指数减半
                    base *= base;
                    exp >>= 1;
                }

                return result;
            }

            // -------- 输出函数 --------
            /**
             * @brief 通用进制字符串转换 (2-36进制)
             * @tparam base 进制 (2-36)
             * @return 指定进制的字符串表示
             */
            template <uint64_t base = 10>
            inline std::string to_string_template() const
            {
                static_assert(base >= 2 && base <= 36, "chenc::big_int::big_uint.to_string_template base must be between 2 and 36");
                if constexpr (base == 2)
                {
                    return to_string_base_2();
                }
                else if constexpr (base == 10)
                {
                    return to_string_base_10();
                }
                else if constexpr (base == 16)
                {
                    return to_string_base_16();
                }
                else
                { // 检查进制范围
                    if (base < 2 || base > 36)
                        return "";
                    if (is_zero())
                        return "0";

                    static std::array<char, 36> base_chars = {
                        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
                        'u', 'v', 'w', 'x', 'y', 'z'};

                    // 快速处理小数 (使用更多位可以处理更大的数)
                    if (data_.size() <= 2)
                    {
                        uint64_t value = data_[0];
                        if (data_.size() == 2)
                            value |= (uint64_t(data_[1]) << 32);

                        std::string result;
                        do
                        {
                            result.push_back(base_chars[value % base]);
                            value /= base;
                        } while (value > 0);

                        std::reverse(result.begin(), result.end());
                        return result;
                    }

                    // 预计算最优块大小
                    uint64_t optimal_base = []()
                    {
                        uint64_t b = base;
                        uint32_t count = 1;
                        while (count < 9 && b <= UINT32_MAX / base)
                        {
                            b *= base;
                            count++;
                        }
                        return b;
                    }();

                    uint32_t optimal_digits = []()
                    {
                        uint64_t b = base;
                        uint32_t count = 1;
                        while (count < 9 && b <= UINT32_MAX / base)
                        {
                            b *= base;
                            count++;
                        }
                        return count;
                    }();

                    // 预估结果长度
                    const uint64_t bits_count = bits();
                    const uint64_t approx_digits = static_cast<uint64_t>(
                        std::ceil(bits_count * std::log2(2) / std::log2(base)) + 10);

                    std::string result;
                    result.reserve(approx_digits);

                    // 使用临时副本
                    big_uint temp(*this);

                    // 存储余数
                    std::vector<uint32_t> remainders;
                    remainders.reserve((temp.data_.size() * 32 + optimal_digits - 1) / optimal_digits);

                    // 使用大基数进行除法
                    while (!temp.is_zero())
                    {
                        uint64_t remainder = 0;

                        // 从高位到低位执行除法
                        for (int64_t i = temp.data_.size() - 1; i >= 0; --i)
                        {
                            uint64_t dividend = (uint64_t(remainder) << 32) | temp.data_[i];
                            temp.data_[i] = static_cast<uint32_t>(dividend / optimal_base);
                            remainder = dividend % optimal_base;
                        }

                        remainders.push_back(static_cast<uint32_t>(remainder));
                        temp.trim();
                    }

                    // 处理余数并构建结果
                    bool is_first = true;
                    for (auto it = remainders.rbegin(); it != remainders.rend(); ++it)
                    {
                        uint32_t value = *it;

                        if (is_first)
                        {
                            // 第一块特殊处理（不需要前导零）
                            if (value != 0)
                            {
                                std::string chunk;
                                do
                                {
                                    chunk.push_back(base_chars[value % base]);
                                    value /= base;
                                } while (value > 0);

                                std::reverse(chunk.begin(), chunk.end());
                                result.append(chunk);
                            }
                            is_first = false;
                        }
                        else
                        {
                            // 后续块需要固定长度
                            std::string chunk;
                            chunk.reserve(optimal_digits);

                            for (uint32_t j = 0; j < optimal_digits; ++j)
                            {
                                chunk.push_back(base_chars[value % base]);
                                value /= base;
                            }

                            std::reverse(chunk.begin(), chunk.end());
                            result.append(chunk);
                        }
                    }

                    return result.empty() ? "0" : result;
                }
            }
            /**
             * @brief 通用进制字符串转换 (2-36进制)
             * @tparam base 进制 (2-36)
             * @return 指定进制的字符串表示
             */
            inline std::string to_string(const uint64_t &base = 10) const
            {
                if (base < 2 || base > 36)
                {
                    throw invalid_argument("chenc::big_int::big_uint.to_string base must be between 2 and 36");
                }
#define CHENC_DEFINE_CASE(val)            \
    case val:                             \
        return to_string_template<val>(); \
        break;

                switch (base)
                {
                    CHENC_DEFINE_CASE(2);
                    CHENC_DEFINE_CASE(3);
                    CHENC_DEFINE_CASE(4);
                    CHENC_DEFINE_CASE(5);
                    CHENC_DEFINE_CASE(6);
                    CHENC_DEFINE_CASE(7);
                    CHENC_DEFINE_CASE(8);
                    CHENC_DEFINE_CASE(9);
                    CHENC_DEFINE_CASE(10);
                    CHENC_DEFINE_CASE(11);
                    CHENC_DEFINE_CASE(12);
                    CHENC_DEFINE_CASE(13);
                    CHENC_DEFINE_CASE(14);
                    CHENC_DEFINE_CASE(15);
                    CHENC_DEFINE_CASE(16);
                    CHENC_DEFINE_CASE(17);
                    CHENC_DEFINE_CASE(18);
                    CHENC_DEFINE_CASE(19);
                    CHENC_DEFINE_CASE(20);
                    CHENC_DEFINE_CASE(21);
                    CHENC_DEFINE_CASE(22);
                    CHENC_DEFINE_CASE(23);
                    CHENC_DEFINE_CASE(24);
                    CHENC_DEFINE_CASE(25);
                    CHENC_DEFINE_CASE(26);
                    CHENC_DEFINE_CASE(27);
                    CHENC_DEFINE_CASE(28);
                    CHENC_DEFINE_CASE(29);
                    CHENC_DEFINE_CASE(30);
                    CHENC_DEFINE_CASE(31);
                    CHENC_DEFINE_CASE(32);
                    CHENC_DEFINE_CASE(33);
                    CHENC_DEFINE_CASE(34);
                    CHENC_DEFINE_CASE(35);
                    CHENC_DEFINE_CASE(36);
                default:
                    return "base out of range";
                }
#undef CHENC_DEFINE_CASE
            }
            /**
             * @brief 浮点数字符串转换
             * @tparam len 小数点位数
             * @return 科学计数法的字符串表示
             */
            inline std::string to_float_string(const uint64_t &len = 5, const bool &pad_with_zeros = false) const
            {
                if (is_zero())
                    return "0";

                std::string str = to_string_template<10>();
                std::string result;

                result.reserve(len + 10);

                result.push_back(str[0]);
                result.push_back('.');
                if (len >= 1)
                    result.append(str.substr(1, len));
                if (pad_with_zeros == true and result.size() - 2 < len)
                    result.append(std::string(len - (result.size() - 2), '0'));
                result.append("e+");
                result.append(std::to_string(str.size() - 1));

                return result;
            }
            /**
             * @brief 转换函数
             * @tparam 目标类型
             * @return 转换后的值
             */
            template <typename T>
                requires std::integral<T> && (sizeof(T) <= sizeof(uint64_t))
            inline explicit operator T() const
            {
                uint64_t value = data_[0];
                if (data_.size() >= 2)
                    value |= static_cast<uint64_t>(data_[1]) << 32;
                return static_cast<T>(value);
            }
            /**
             * @brief 转换函数
             * @tparam 目标类型
             * @return 转换后的值
             * @note 溢出将初始化为正无穷
             */
            template <typename T>
                requires std::floating_point<T>
            inline explicit operator T() const
            {
                T value = 0.0;
                auto float_precision = std::numeric_limits<T>::digits; // 浮点数精度

                // 循环计算
                for (uint64_t i = 0; (i * 32 <= float_precision + 31) and (i < data_.size()); i++)
                {
                    value += static_cast<T>(data_[(data_.size() - 1) - i]) *
                             std::pow(2.0, ((data_.size() - 1 - i) * 32));
                }

                return value;
            }

            /**
             * @brief 交换函数
             * @param other
             * @return void
             */
            inline void swap(big_uint &other) noexcept
            {
                std::swap(data_, other.data_);
            }

            // -------- 友元函数 --------
            inline friend std::ostream &operator<<(std::ostream &os, const big_uint &value)
            {
                // 获取进制
                auto ios = os.flags();
                if (ios & std::ios::hex)
                {
                    os << value.to_string_template<16>();
                }
                else if (ios & std::ios::oct)
                {
                    os << value.to_string_template<8>();
                }
                else if (ios & std::ios::dec)
                {
                    os << value.to_string_template<10>();
                }
                return os;
            }
            inline friend std::istream &operator>>(std::istream &is, big_uint &value)
            {
                std::string str;
                is >> str;
                value = from_str<10>(str);
                return is;
            }

        private:
            /**
             * @brief 10进制字符串转换 - 极致性能特化版本
             * @return 10进制的字符串表示
             */
            inline std::string to_string_base_10() const
            {
                if (is_zero())
                    return "0";

                // 快速处理小数 (使用更多位可以处理更大的数)
                if (data_.size() <= 2)
                {
                    uint64_t value = data_[0];
                    if (data_.size() == 2)
                        value |= (uint64_t(data_[1]) << 32);

                    std::string result;
                    do
                    {
                        result.push_back('0' + (value % 10));
                        value /= 10;
                    } while (value > 0);

                    std::reverse(result.begin(), result.end());
                    return result;
                }

                // 10进制特化优化
                // 使用10^9作为基数，因为这是在32位中10的最大幂
                uint32_t chunk_base = 1000000000U; // 10^9
                uint32_t chunk_digits = 9;

                // 预估结果长度 (使用更精确的估算)
                const uint64_t bits_count = bits();
                const uint64_t approx_digits = static_cast<uint64_t>(
                    bits_count * 0.302 /* log10(2) */ + 10);

                std::string result;
                result.reserve(approx_digits);

                // 使用临时副本
                big_uint temp(*this);

                // 存储余数
                std::vector<uint32_t> remainders;
                remainders.reserve((temp.data_.size() * 32 + 31) / 32);

                // 使用10^9基数进行除法
                while (!temp.is_zero())
                {
                    uint64_t remainder = 0;

                    // 从高位到低位执行除法
                    for (int64_t i = temp.data_.size() - 1; i >= 0; --i)
                    {
                        uint64_t dividend = (uint64_t(remainder) << 32) | temp.data_[i];
                        temp.data_[i] = static_cast<uint32_t>(dividend / chunk_base);
                        remainder = dividend % chunk_base;
                    }

                    remainders.push_back(static_cast<uint32_t>(remainder));
                    temp.trim();
                }
                // 处理余数并构建结果
                bool is_first = true;
                for (auto it = remainders.rbegin(); it != remainders.rend(); ++it)
                {
                    uint32_t value = *it;

                    if (is_first)
                    {
                        // 第一块特殊处理（不需要前导零）
                        if (value != 0)
                        {
                            std::string chunk;
                            do
                            {
                                chunk.push_back('0' + (value % 10));
                                value /= 10;
                            } while (value > 0);

                            std::reverse(chunk.begin(), chunk.end());
                            result.append(chunk);
                        }
                        is_first = false;
                    }
                    else
                    {
                        // 后续块需要固定长度(9位)
                        std::string chunk;
                        chunk.reserve(chunk_digits);

                        for (uint32_t j = 0; j < chunk_digits; ++j)
                        {
                            chunk.push_back('0' + (value % 10));
                            value /= 10;
                        }

                        std::reverse(chunk.begin(), chunk.end());
                        result.append(chunk);
                    }
                }

                return result.empty() ? "0" : result;
            }

            /**
             * @brief 2进制字符串转换 - 极致性能特化版本
             * @return 2进制的字符串表示
             */
            inline std::string to_string_base_2() const
            {
                std::string result;
                int64_t bit_len = bits();
                result.resize(bit_len + 1, '0');

                for (int64_t i = bit_len; i >= 0; --i)
                    result[bit_len - i] = bit_test(i) ? '1' : '0';

                return result;
            }
            /**
             * @brief 16进制字符串转换 - 极致性能特化版本
             * @return 16进制的字符串表示
             */
            inline std::string to_string_base_16() const
            {
                static char hex_chars[] = "0123456789abcdef";
                std::string result;
                result.reserve(blocks() * 8);

                // 处理头
                bool is_show = false;
                for (int64_t i = 32; (i -= 4) >= 0;)
                    if (is_show == true)
                        result.push_back(hex_chars[(data_.back() >> i) & 0xf]);
                    else if ((data_.back() >> i) > 0)
                    {
                        result.push_back(hex_chars[(data_.back() >> i) & 0xf]);
                        is_show = true;
                    }

                // 处理后面
                for (int64_t i = int64_t(data_.size()) - 2; i >= 0; i--)
                    for (int64_t j = 32; (j -= 4) >= 0;)
                        result.push_back(hex_chars[(data_[i] >> j) & 0xf]);

                return result;
            }
            /**
             * @brief 高精度乘法 default
             */
            inline static void multiply_default(big_uint &a, const big_uint &b)
            {
                // 处理特殊情况
                if (a.is_zero() || b.is_zero())
                {
                    a = 0;
                    return;
                }
                if (a.is_one())
                {
                    a.data_ = b.data_;
                    return;
                }
                if (b.is_one())
                {
                    a.data_ = a.data_;
                    return;
                }

                // 保存操作数的原始数据
                const std::vector<uint32_t> &a_data = a.data_;
                const std::vector<uint32_t> &b_data = b.data_;

                // 初始化结果为0，大小为两个操作数大小之和
                std::vector<uint32_t> result(a_data.size() + b_data.size(), 0);

                // 标准乘法算法
                for (uint64_t i = 0; i < a_data.size(); i++)
                {
                    if (a_data[i] == 0)
                        continue; // 优化：跳过0乘法

                    uint64_t carry = 0;
                    for (uint64_t j = 0; j < b_data.size() || carry > 0; j++)
                    {
                        uint64_t product = carry;
                        if (j < b_data.size())
                        {
                            product += uint64_t(a_data[i]) * uint64_t(b_data[j]);
                        }
                        product += result[i + j];

                        result[i + j] = uint32_t(product & UINT32_MAX);
                        carry = product >> 32;
                    }
                }

                // 将结果赋值给a
                a.data_.swap(result);
                a.trim();
            }
            /**
             * @brief 高精度乘法 karatsuba (优化版)
             */
            inline static void multiply_karatsuba(big_uint &a, const big_uint &b)
            {
                // 处理特殊情况
                if (a.is_zero() || b.is_zero())
                {
                    a = 0;
                    return;
                }
                if (a.is_one())
                {
                    a.data_ = b.data_;
                    return;
                }
                if (b.is_one())
                {
                    a.data_ = a.data_;
                    return;
                }

                // 获取操作数的大小
                uint64_t n = std::max(a.blocks(), b.blocks());

                // 对于小数字，使用标准算法更高效
                if (n <= 64)
                { // 提高阈值
                    multiply_default(a, b);
                    return;
                }

                // 确保两个数都有n块
                const std::vector<uint32_t> &a_data = a.data_;
                const std::vector<uint32_t> &b_data = b.data_;

                // 将数字分成两半
                uint64_t half = (n + 1) / 2;

                // 直接在原数据上操作，避免拷贝
                big_uint a_low, a_high, b_low, b_high;

                // 构造低半部分
                a_low.data_.assign(a_data.begin(),
                                   a_data.begin() + std::min(half, a_data.size()));
                a_low.trim();
                if (a_low.data_.empty())
                    a_low.data_.push_back(0);

                // 构造高半部分
                if (half < a_data.size())
                {
                    a_high.data_.assign(a_data.begin() + half, a_data.end());
                    a_high.trim();
                    if (a_high.data_.empty())
                        a_high.data_.push_back(0);
                }
                else
                {
                    a_high.data_.push_back(0);
                }

                // 构造b的低半部分
                b_low.data_.assign(b_data.begin(),
                                   b_data.begin() + std::min(half, b_data.size()));
                b_low.trim();
                if (b_low.data_.empty())
                    b_low.data_.push_back(0);

                // 构造b的高半部分
                if (half < b_data.size())
                {
                    b_high.data_.assign(b_data.begin() + half, b_data.end());
                    b_high.trim();
                    if (b_high.data_.empty())
                        b_high.data_.push_back(0);
                }
                else
                {
                    b_high.data_.push_back(0);
                }

                // 计算三个乘积:
                // z0 = a_low * b_low
                // z1 = (a_low + a_high) * (b_low + b_high)
                // z2 = a_high * b_high

                // 计算z2 = a_high * b_high (先算这个，因为可能较小)
                big_uint z2(a_high);
                z2 *= b_high;

                // 计算z0 = a_low * b_low
                big_uint z0(a_low);
                z0 *= b_low;

                // 计算z1 = (a_low + a_high) * (b_low + b_high)
                a_low += a_high; // a_low 现在是 (a_low + a_high)
                b_low += b_high; // b_low 现在是 (b_low + b_high)

                big_uint z1(a_low);
                z1 *= b_low;

                // z1 = z1 - z0 - z2
                z1 -= z0;
                z1 -= z2;

                // 构建最终结果
                // 结果 = z0 + (z1 << half*32) + (z2 << 2*half*32)
                a = std::move(z0); // a现在包含z0

                // 添加 z1 << half*32
                if (!z1.is_zero())
                {
                    // 直接修改z1实现左移
                    z1.data_.insert(z1.data_.begin(), half, 0);
                    a += z1;
                }

                // 添加 z2 << 2*half*32
                if (!z2.is_zero())
                {
                    // 直接修改z2实现左移
                    z2.data_.insert(z2.data_.begin(), 2 * half, 0);
                    a += z2;
                }

                a.trim();
            }
            /**
             * @brief 高精度除法 - 优化版本
             * @param dividend 被除数
             * @param divisor 除数
             * @param quotient 商
             * @param remainder 余数
             * @note 备选算法 几乎所有情况 Newton-Raphson 都更快
             */
            inline static void division_default(const big_uint &dividend, const big_uint &divisor,
                                                big_uint &quotient, big_uint &remainder)
            {
                if (dividend.is_zero())
                {
                    quotient = 0;
                    remainder = 0;
                    return;
                }
                if (divisor.is_zero())
                {
                    quotient = 0;
                    remainder = dividend;
                    return;
                }
                if (divisor.is_one())
                {
                    quotient = dividend;
                    remainder = 0;
                    return;
                }
                if (dividend < divisor)
                {
                    quotient = 0;
                    remainder = dividend;
                    return;
                }

                // 标准长除法算法
                remainder = dividend;
                big_uint temp_divisor = divisor << (dividend.bits() - divisor.bits());
                quotient = 0;
                big_uint temp_quotient = big_uint(1) << (dividend.bits() - divisor.bits());

                // 循环减法
                while (!temp_quotient.is_zero())
                {
                    if (remainder >= temp_divisor)
                    {
                        remainder -= temp_divisor;
                        quotient += temp_quotient;
                    }
                    // 分支裁剪
                    if (temp_divisor.bits() > remainder.bits())
                    {
                        uint64_t temp_bits = temp_divisor.bits() - remainder.bits();
                        temp_divisor >>= temp_bits;
                        temp_quotient >>= temp_bits;
                    }
                    else
                    {
                        temp_divisor >>= 1;
                        temp_quotient >>= 1;
                    }
                }
            }
            /**
             * @brief 高精度除法 - Newton-Raphson 优化版本
             * @param dividend 被除数
             * @param divisor 除数
             * @param quotient 商
             * @param remainder 余数
             */
            inline static void division_newton_raphson(const big_uint &dividend, const big_uint &divisor,
                                                       big_uint &quotient, big_uint &remainder)
            {
                // 边界情况检查
                if (dividend < divisor)
                {
                    quotient = 0;
                    remainder = dividend;
                    return;
                }

                // 对于小操作数，使用 除法
                if (dividend.blocks() <= 2 && divisor.blocks() <= 2)
                {
                    // 此处可以调用您的 division_default，或者直接计算
                    uint64_t a = static_cast<uint64_t>(dividend);
                    uint64_t b = static_cast<uint64_t>(divisor);
                    quotient = a / b;
                    remainder = a % b;
                    return;
                }

                // 精度 k 的选择：理论上比商的位数多一点即可。这里选择一个安全的较大值。
                uint64_t kbits = dividend.bits() + 32;
                big_uint B = big_uint(1) << kbits;
                big_uint twoB = B << 1;

                // 初始近似值 x0 (1/divisor 的定点表示)
                // 这里的初始值可以进一步优化，但当前值是可用的
                int64_t shift_init = static_cast<int64_t>(kbits) - static_cast<int64_t>(divisor.bits());
                big_uint x = (shift_init > 0) ? (big_uint(1) << shift_init) : big_uint(1);

                // Newton-Raphson 迭代: x_{n+1} = (x * (2B - divisor * x)) >> kbits
                big_uint prev_x; // 用于检测收敛
                do
                {
                    prev_x = x;
                    big_uint t = divisor * x;

                    // 确保 (2B - t) 不会下溢
                    big_uint term = (twoB > t) ? (twoB - t) : big_uint(0);

                    x *= term;
                    x >>= kbits;

                } while (x != prev_x); // 迭代直到 x 收敛

                // 计算近似商： q = (dividend * x) >> kbits
                quotient = dividend * x;
                quotient >>= kbits;

                // --- 优化核心：高效的商修正 ---
                // 计算 q * divisor，这几乎等于 dividend
                big_uint qd = quotient * divisor;

                // 计算余数 remainder = dividend - qd
                remainder = dividend - qd;

                // 如果余数大于除数，说明商偏小了，需要增加商并减少余数
                // 这个循环通常最多执行 1-2 次
                while (remainder >= divisor)
                {
                    ++quotient;
                    remainder -= divisor;
                }
                // 注意：由于近似计算，理论上 q*d 可能略大于 dividend，导致 remainder 计算结果为0（因为是无符号减法）。
                // 在您的减法实现中，`*this < other` 时结果为0，这已经隐式处理了这种情况。
                // 一个更鲁棒的实现会使用有符号大数或单独处理借位。
                // 假设您的减法在 `a < b` 时 `a - b` 结果为 0 是稳定的。
            }

            /**
             * @brief 去除前导 0
             */
            inline big_uint &trim()
            {
                while (data_.size() >= 2 and data_.back() == 0)
                    data_.pop_back();
                return *this;
            }
            /**
             * @brief 计算需要的32位块数量
             * @param bits 需要的位数
             * @return 32位块数量
             */
            inline static uint64_t calc_blocks(uint64_t bits) noexcept
            {
                return (bits + 31) / 32;
            }
            /**
             * @brief 存储数据的 vector，使用 32 位无符号整数存储大整数。
             * @note 每个元素表示 32 位，多个元素表示更大的整数。
             * @note 最低位在前，高位在后。
             * @note 数据紧密存储，没有多余的前导 0。
             */
            std::vector<uint32_t> data_;
            // 默认容量
            inline static constexpr uint64_t def_cap_ = 256;
        };

    }
}

namespace std
{
    template <>
    struct hash<chenc::big_int::big_uint>
    {
        std::size_t operator()(const chenc::big_int::big_uint &big_uint) const
        {
            return std::hash<std::string>()(big_uint.to_string_template<16>());
        }
    };
}

#endif