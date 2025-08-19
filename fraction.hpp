#ifndef CHENC_FRACTION_HPP
#define CHENC_FRACTION_HPP

#include "big_uint.hpp"

#include <concepts>
#include <cstdint>
#include <limits>
#include <cmath>
#include <bit>

#include "../time/time.hpp" 

namespace chenc::tools
{
    enum class analyzing_floating_point_enum
    {
        // 正常
        normal,
        // 零
        zero,
        // 无穷
        infinity,
        // 非数字
        not_a_number,
    };

    /**
     * @brief 解析浮点数指数和尾数和符号
     * @param value 待解析的浮点数
     * @tparam FLOAT_TYPE 浮点数类型
     * @note 此函数假定平台使用小端字节序 (Little-Endian)
     * @note 对于 128-bit 和 80-bit 类型，字节序问题会影响结果的正确性
     * @note max_bits 最大精度限制为64时，精度已远大于双精度浮点数，默认256则更远远大于正常浮点数
     * @note 十进制有效精度计算公式 log2(64)≈19.2 log2(256)≈77 log2(512)≈154 log2(1024)≈308 log2(2048)≈616
     * @note 实际为避免中间计算溢出精度，应预留足够的有效精度
     */
    template <typename FLOAT_TYPE>
        requires std::floating_point<FLOAT_TYPE>
    inline constexpr analyzing_floating_point_enum analyzing_floating_point(const FLOAT_TYPE &value, bool &is_negative, int64_t &exponent, uint64_t &mantissa_low, uint64_t &mantissa_high)
    {
        // 首先处理 NaN 和 Infinity，因为它们的判断不依赖于具体的位解析
        if (std::isnan(value))
        {
            // is_negative 对于 NaN 没有标准定义，这里保持其默认值
            return analyzing_floating_point_enum::not_a_number;
        }
        if (std::isinf(value))
        {
            // 对于无穷大，我们可以轻易地判断其符号
            is_negative = (value < 0);
            return analyzing_floating_point_enum::infinity;
        }

        // IEEE 754 16-bit 半精度浮点数 (例如 std::float16_t)
        if constexpr (std::numeric_limits<FLOAT_TYPE>::max_exponent == 16 &&
                      std::numeric_limits<FLOAT_TYPE>::digits == 11)
        {
            // 使用 std::bit_cast 避免未定义行为
            const auto bits = std::bit_cast<uint16_t>(value);

            is_negative = (bits >> 15) != 0;
            exponent = (bits >> 10) & 0x1F; // 5 bits for exponent
            mantissa_low = bits & 0x3FF;    // 10 bits for mantissa
            mantissa_high = 0;

            constexpr int bias = 15;

            // 检查是否为零 (解决了 -0.0 的问题)
            if (exponent == 0 && mantissa_low == 0)
            {
                return analyzing_floating_point_enum::zero;
            }

            // 检查是否为次正规数 (指数全0，但尾数不为0)
            if (exponent == 0)
            {
                exponent = 1 - bias; // 次正规数的指数
            }
            else
            {
                exponent -= bias;                    // 正常数的指数
                mantissa_low |= (uint64_t(1) << 10); // 添加隐含的1
            }
            return analyzing_floating_point_enum::normal;
        }
        // IEEE 754 32-bit 单精度浮点数 (float)
        else if constexpr (std::numeric_limits<FLOAT_TYPE>::max_exponent == 128 &&
                           std::numeric_limits<FLOAT_TYPE>::digits == 24)
        {
            const auto bits = std::bit_cast<uint32_t>(value);

            is_negative = (bits >> 31) != 0;
            exponent = (bits >> 23) & 0xFF; // 8 bits
            mantissa_low = bits & 0x7FFFFF; // 23 bits
            mantissa_high = 0;

            constexpr int bias = 127;

            if (exponent == 0 && mantissa_low == 0)
            {
                return analyzing_floating_point_enum::zero;
            }

            if (exponent == 0)
            {
                exponent = 1 - bias;
            }
            else
            {
                exponent -= bias;
                mantissa_low |= (uint64_t(1) << 23);
            }
            return analyzing_floating_point_enum::normal;
        }
        // IEEE 754 64-bit 双精度浮点数 (double)
        else if constexpr (std::numeric_limits<FLOAT_TYPE>::max_exponent == 1024 &&
                           std::numeric_limits<FLOAT_TYPE>::digits == 53)
        {
            const auto bits = std::bit_cast<uint64_t>(value);

            is_negative = (bits >> 63) != 0;
            exponent = (bits >> 52) & 0x7FF;       // 11 bits
            mantissa_low = bits & 0xFFFFFFFFFFFFF; // 52 bits
            mantissa_high = 0;

            constexpr int bias = 1023;

            if (exponent == 0 && mantissa_low == 0)
            {
                return analyzing_floating_point_enum::zero;
            }

            if (exponent == 0)
            {
                exponent = 1 - bias;
            }
            else
            {
                exponent -= bias;
                mantissa_low |= (uint64_t(1) << 52);
            }
            return analyzing_floating_point_enum::normal;
        }
        // IEEE 754 128-bit 四精度浮点数 (例如 std::float128_t)
        else if constexpr (std::numeric_limits<FLOAT_TYPE>::max_exponent == 16384 &&
                           std::numeric_limits<FLOAT_TYPE>::digits == 113)
        {
            static_assert(sizeof(FLOAT_TYPE) == 16, "128-bit float is expected to be 16 bytes.");
            const auto bits = std::bit_cast<std::array<uint64_t, 2>>(value);
            const uint64_t low_bits = bits[0];
            const uint64_t high_bits = bits[1];

            is_negative = (high_bits >> 63) != 0;
            exponent = (high_bits >> 48) & 0x7FFF; // 15 bits
            mantissa_low = low_bits;
            mantissa_high = high_bits & 0xFFFFFFFFFFFF; // 48 bits

            constexpr int bias = 16383;

            if (exponent == 0 && mantissa_low == 0 && mantissa_high == 0)
            {
                return analyzing_floating_point_enum::zero;
            }

            if (exponent == 0)
            {
                exponent = 1 - bias;
            }
            else
            {
                exponent -= bias;
                mantissa_high |= (uint64_t(1) << 48);
            }
            return analyzing_floating_point_enum::normal;
        }
        // 扩展双精度浮点数 (long double, 80-bit on x86)
        // 警告：这种格式的内存布局没有标准化，这里的实现可能不具备完全的可移植性
        else if constexpr (std::numeric_limits<FLOAT_TYPE>::max_exponent == 16384 &&
                           std::numeric_limits<FLOAT_TYPE>::digits == 64)
        {
            // 80-bit long double 通常被填充到12或16字节，使用 memcpy 或 bit_cast 到字节数组是更安全的方式
            // 这里我们假设它被填充到16字节，并且使用 bit_cast 到两个 uint64_t
            static_assert(sizeof(FLOAT_TYPE) >= 10, "80-bit extended precision float is expected to be at least 10 bytes.");
            // 假设在小端系统上，低80位是有效数据
            const auto bits = std::bit_cast<std::array<uint64_t, sizeof(FLOAT_TYPE) / 8>>(value);
            const uint64_t mantissa_bits = bits[0];
            const uint64_t exp_sign_bits = bits[1];

            is_negative = (exp_sign_bits >> 15) != 0;
            exponent = exp_sign_bits & 0x7FFF; // 15 bits, no sign bit in this part
            mantissa_low = mantissa_bits;
            mantissa_high = 0;

            constexpr int bias = 16383;

            if (exponent == 0 && mantissa_low == 0)
            {
                return analyzing_floating_point_enum::zero;
            }

            // 80-bit 格式的整数位是显式的 (bit 63 of mantissa_low), 不像其他格式是隐含的
            // 只有当指数不为0 (normal or pseudo-denormal) 且整数位为1时，才是正常数
            if (exponent == 0)
            {
                // 次正规数 (integer part is 0)
                exponent = 1 - bias;
            }
            else
            {
                // 正常数 (integer part must be 1)
                exponent -= bias;
            }
            return analyzing_floating_point_enum::normal;
        }
        // 默认情况：未知或不支持的浮点类型
        else
        {
            // 使用 static_assert 在编译时报错，而不是在运行时返回错误的值
            static_assert(sizeof(FLOAT_TYPE) == 0, "Unsupported floating-point type.");
            return analyzing_floating_point_enum::not_a_number; // 使编译器满意
        }
    }
}

namespace chenc::big_int
{
    /**
     * @class fraction
     * @brief 分数类，支持有理数运算
     * @note 使用 big_uint 作为底层实现，支持高精度分数运算
     */
    class fraction
    {
    private:
        using big_uint = chenc::big_int::big_uint;

    public:
        /**
         * @brief 默认构造函数，构造值为0的分数
         */
        inline fraction(const uint64_t &max_bits = 256)
            : numerator_(0), denominator_(1), is_negative_(false), max_bits_(max_bits)
        {
        }

        /**
         * @brief 从分子和分母构造分数
         * @param numerator 分子
         * @param denominator 分母
         * @param max_bits 最大精度限制
         */
        inline fraction(const big_uint &numerator, const big_uint &denominator, const uint64_t &max_bits = 256)
            : numerator_(numerator), denominator_(denominator), is_negative_(false), max_bits_(max_bits)
        {
            // 化简分数
            simplify();
        }

        /**
         * @brief 从整数类型构造分数
         * @tparam T1 分子类型（整数）
         * @tparam T2 分母类型（整数）
         * @param numerator 分子
         * @param denominator 分母
         * @param max_bits 最大精度限制
         */
        template <typename T1, typename T2>
            requires std::integral<T1> && (sizeof(T1) <= sizeof(uint64_t)) && std::integral<T2> && (sizeof(T2) <= sizeof(uint64_t))
        inline fraction(const T1 &numerator, const T2 &denominator, const uint64_t &max_bits = 256)
            : numerator_(numerator), denominator_(denominator), is_negative_((numerator < 0) ^ (denominator < 0)), max_bits_(max_bits)
        {
            numerator_ = numerator;
            denominator_ = denominator;

            // 化简分数
            simplify();
        }

        /**
         * @brief 从浮点数构造分数
         * @tparam T1 浮点数类型
         * @param value 浮点数值
         * @param max_bits 最大精度限制
         */
        template <typename T1>
            requires std::floating_point<T1>
        inline fraction(const T1 &value, const uint64_t &max_bits = 256)
            : max_bits_(max_bits)
        {
            uint64_t mantissa[2];
            int64_t exponent;
            bool is_negative;
            auto error = chenc::tools::analyzing_floating_point<T1>(value, is_negative, exponent, mantissa[0], mantissa[1]);
            if (error == chenc::tools::analyzing_floating_point_enum::zero)
            {
                numerator_ = 0;
                denominator_ = 1;
                is_negative = false;
                return;
            }
            else if (error != chenc::tools::analyzing_floating_point_enum::normal)
                throw chenc::big_int::invalid_argument("chenc::big_int::fraction.fraction float value is invalid");

            numerator_ = {mantissa[0], mantissa[1]};
            denominator_ = 1;
            denominator_ <<= std::numeric_limits<T1>::digits - 1;

            // std:: cout << "numerator: " << numerator_ << "\n";
            // std:: cout << "denominator: " << denominator_ << "\n";
            // std::cout << "exponent: " << exponent << "\n"<< "\n";

            is_negative_ = is_negative;
            if (exponent > 0)
            {
                numerator_ <<= chenc::tools::abs(exponent);
            }
            else if (exponent < 0)
            {
                denominator_ <<= chenc::tools::abs(exponent);
            }
            // exponent == 0 不做处理

            // std:: cout << "numerator: " << numerator_ << "\n";
            // std:: cout << "denominator: " << denominator_ << "\n";
            // std::cout << "exponent: " << exponent << "\n"<< "\n";

            // 化简分数
            simplify();
        }

        /**
         * @brief 从字符串构造
         * @param str
         * @param max_bits 最大精度限制
         * @note 字符串需要为 科学计数法 或 小数格式 或 整数格式
         */
        inline fraction(const std::string_view &str, const uint64_t &max_bits = 256)
            : max_bits_(max_bits)
        {
            if (str.empty())
                throw chenc::big_int::invalid_argument("chenc::big_int::fraction.fraction string cannot be empty");

            // 1.13e3
            // 1.13e+3
            // 1.13e-3
            // 1.e-3
            // -1.13e+3
            // 2149.1413
            // +2149.1413
            // -2149.1413
            // 也可以是整数，如 "123" 或 "-456"

            int64_t char_e_index = -1;                      // 字符 e 的索引
            int64_t char_dot_index = -1;                    // 字符 . 的索引
            int64_t char_plus_or_minus_index[2] = {-1, -1}; // 加号或减号索引

            // 获取字符索引
            for (uint64_t i = 0; i < str.size(); i++)
            {
                switch (str[i])
                {
                case '+':
                case '-':
                    if (char_plus_or_minus_index[0] < 0)
                        char_plus_or_minus_index[0] = i;
                    else if (char_plus_or_minus_index[1] < 0)
                        char_plus_or_minus_index[1] = i;
                    else
                        throw chenc::big_int::invalid_argument("chenc::big_int::fraction.fraction string can only contain two plus or minus signs");
                    break;
                case '.':
                    if (char_dot_index < 0)
                        char_dot_index = i;
                    else
                        throw chenc::big_int::invalid_argument("chenc::big_int::fraction.fraction string can only contain one dot");
                    break;
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    break;
                case 'e':
                case 'E':
                    if (char_e_index < 0)
                        char_e_index = i;
                    else
                        throw chenc::big_int::invalid_argument("chenc::big_int::fraction.fraction string can only contain one e");
                    break;
                default:
                    throw chenc::big_int::invalid_argument("chenc::big_int::fraction.fraction string contains invalid character");
                }
            }

            // 处理符号
            is_negative_ = false;
            uint64_t start_index = 0;
            if (char_plus_or_minus_index[0] == 0)
            {
                is_negative_ = (str[0] == '-');
                start_index = 1;
            }

            // 科学计数法
            if (char_e_index >= 0)
            {
                // 验证e后面必须有指数
                if (char_e_index + 1 >= str.size())
                    throw chenc::big_int::invalid_argument("chenc::big_int::fraction.fraction missing exponent after 'e'");

                // 解析指数部分的符号
                int64_t exponent_sign_index = -1;
                if (str[char_e_index + 1] == '+' || str[char_e_index + 1] == '-')
                {
                    exponent_sign_index = char_e_index + 1;
                }

                // 验证指数部分有数字
                uint64_t exponent_start = char_e_index + 1 + (exponent_sign_index > 0 ? 1 : 0);
                if (exponent_start >= str.size())
                    throw chenc::big_int::invalid_argument("chenc::big_int::fraction.fraction missing exponent value");

                // 验证指数部分都是数字
                for (uint64_t i = exponent_start; i < str.size(); i++)
                {
                    if (str[i] < '0' || str[i] > '9')
                        throw chenc::big_int::invalid_argument("chenc::big_int::fraction.fraction invalid character in exponent");
                }

                // 解析整数部分
                std::string integer_part;
                if (char_dot_index >= 0)
                {
                    // 有小数点
                    integer_part = std::string(str.substr(start_index, char_dot_index - start_index));
                }
                else
                {
                    // 没有小数点
                    integer_part = std::string(str.substr(start_index, char_e_index - start_index));
                }

                // 解析小数部分
                std::string fractional_part;
                if (char_dot_index >= 0)
                {
                    fractional_part = std::string(str.substr(char_dot_index + 1, char_e_index - char_dot_index - 1));
                }

                // 解析指数部分
                std::string exponent_str = std::string(str.substr(exponent_start));
                int64_t exponent = std::stoll(exponent_str);
                if (exponent_sign_index > 0 && str[exponent_sign_index] == '-')
                    exponent = -exponent;

                // 构造分数
                // 先构造基本数值（不考虑指数）
                std::string number_str = integer_part + fractional_part;
                if (number_str.empty())
                    number_str = "0";

                numerator_ = big_uint(number_str);
                denominator_ = 1;

                // 处理小数点后的位数
                if (!fractional_part.empty())
                {
                    // 分母需要乘以10的fractional_part.length()次方
                    big_uint decimal_places_multiplier = 1;
                    for (size_t i = 0; i < fractional_part.length(); i++)
                        decimal_places_multiplier *= 10;
                    denominator_ = decimal_places_multiplier;
                }

                // 处理指数
                if (exponent > 0)
                {
                    // 正指数：分子乘以10^exponent
                    big_uint multiplier = 1;
                    for (int64_t i = 0; i < exponent; i++)
                        multiplier *= 10;
                    numerator_ *= multiplier;
                }
                else if (exponent < 0)
                {
                    // 负指数：分母乘以10^abs(exponent)
                    big_uint multiplier = 1;
                    for (int64_t i = 0; i < -exponent; i++)
                        multiplier *= 10;
                    denominator_ *= multiplier;
                }

                // 化简分数
                simplify();
            }
            // 普通小数
            else
            {
                // 解析整数部分
                std::string integer_part;
                if (char_dot_index >= 0)
                {
                    integer_part = std::string(str.substr(start_index, char_dot_index - start_index));
                }
                else
                {
                    integer_part = std::string(str.substr(start_index));
                }

                // 解析小数部分
                std::string fractional_part;
                if (char_dot_index >= 0)
                {
                    fractional_part = std::string(str.substr(char_dot_index + 1));
                }

                // 构造分数
                std::string number_str = integer_part + fractional_part;
                if (number_str.empty())
                    number_str = "0";

                numerator_ = big_uint(number_str);
                denominator_ = 1;

                // 处理小数部分
                if (!fractional_part.empty())
                {
                    // 分母需要乘以10的fractional_part.length()次方
                    big_uint decimal_places_multiplier = 1;
                    for (size_t i = 0; i < fractional_part.length(); i++)
                        decimal_places_multiplier *= 10;
                    denominator_ = decimal_places_multiplier;
                }

                // 化简分数
                simplify();
            }
        }

        /**
         * @brief 拷贝构造函数
         */
        inline fraction(const fraction &value) = default;

        /**
         * @brief 移动构造函数
         */
        inline fraction(fraction &&value) = default;
        /**
         * @brief 拷贝赋值函数
         */
        inline fraction &operator=(const fraction &value)
        {
            numerator_ = value.numerator_;
            denominator_ = value.denominator_;
            is_negative_ = value.is_negative_;
            max_bits_ = std::max(max_bits_, value.max_bits_);
            return *this;
        }

        /**
         * @brief 移动赋值函数
         */
        inline fraction &operator=(fraction &&value)
        {
            numerator_ = std::move(value.numerator_);
            denominator_ = std::move(value.denominator_);
            is_negative_ = value.is_negative_;
            value.is_negative_ = false;
            max_bits_ = std::max(max_bits_, value.max_bits_);
            return *this;
        }
        /**
         * @brief 转换为浮点数
         * @tparam T1 目标浮点数类型
         * @return 转换后的浮点数
         */
        template <typename T1>
            requires std::floating_point<T1>
        inline operator T1() const
        {
            std::string str = to_string(std::numeric_limits<T1>::max_digits10 + 10);
            return std::stold(str);
        }

        /**
         * @brief 是否为 1，不区分正负
         * @return bool
         */
        inline bool is_one() const
        {
            return numerator_ == denominator_;
        }

        /**
         * @brief 是否为 0
         * @return bool
         */
        inline bool is_zero() const
        {
            return numerator_ == 0;
        }

        /**
         * @brief 设置精度
         * @return 自身引用
         */
        inline fraction &set_precision(const uint64_t &value)
        {
            if (value < max_bits_)
                simplify();
            max_bits_ = value;
            return *this;
        }

        /**
         * @brief 转换为科学计数法
         * @param precision 小数点后位数
         * @param pad_with_zeros 是否用零填充至指定精度
         * @return 科学计数法的字符串表示 (例如: 1.2345e+10)
         */
        inline std::string to_string(const uint64_t &precision = 5, const bool &pad_with_zeros = false) const
        {
            if (numerator_.is_zero())
            {
                if (!pad_with_zeros)
                    return "0.e+0";
                else
                    return std::string("0.") + std::string(precision, '0') + "e+0";
            }

            // 整数部分 / 余数
            big_uint int_part, remainder;
            big_uint::div(numerator_, denominator_, int_part, remainder);

            std::string result;
            if (is_negative_)
                result.push_back('-');

            int64_t exponent = 0;

            if (!int_part.is_zero())
            {
                // 整数部分非零
                std::string int_str = int_part.to_string_template<10>();
                exponent = static_cast<int64_t>(int_str.length() - 1);

                // 添加首位
                result.push_back(int_str[0]);
                result.push_back('.');

                // 添加后续整数部分
                uint64_t copied = 0;
                if (int_str.length() > 1)
                {
                    uint64_t avail = std::min<uint64_t>(precision, int_str.length() - 1);
                    result.append(int_str.substr(1, avail));
                    copied += avail;
                }

                // 如果精度不足，继续算余数
                while (copied < precision && !remainder.is_zero())
                {
                    remainder *= 10;
                    big_uint digit, new_rem;
                    big_uint::div(remainder, denominator_, digit, new_rem);
                    result.append(digit.to_string_template<10>());
                    remainder = new_rem;
                    ++copied;
                }

                // 补零
                if (pad_with_zeros)
                {
                    while (copied < precision)
                    {
                        result.push_back('0');
                        ++copied;
                    }
                }
            }
            else
            {
                // 整数部分为零 → 找第一个非零小数
                remainder *= 10;
                big_uint digit, new_rem;
                big_uint::div(remainder, denominator_, digit, new_rem);

                int64_t leading_zeros = 0;
                while (digit.is_zero())
                {
                    remainder = new_rem * 10;
                    big_uint::div(remainder, denominator_, digit, new_rem);
                    ++leading_zeros;
                }

                exponent = -(leading_zeros + 1);

                result.push_back(digit.to_string_template<10>()[0]);
                result.push_back('.');

                uint64_t copied = 0;
                remainder = new_rem;

                while (copied < precision && !remainder.is_zero())
                {
                    remainder *= 10;
                    big_uint d, new_r;
                    big_uint::div(remainder, denominator_, d, new_r);
                    result.append(d.to_string_template<10>());
                    remainder = new_r;
                    ++copied;
                }

                if (pad_with_zeros)
                {
                    while (copied < precision)
                    {
                        result.push_back('0');
                        ++copied;
                    }
                }
            }

            // 删除末尾的零
            if (pad_with_zeros == false)
                while (result.back() == '0')
                    result.pop_back();

            // 拼接指数
            result.push_back('e');
            if (exponent >= 0)
                result.push_back('+');
            else
            {
                result.push_back('-');
                exponent = -exponent;
            }
            result.append(std::to_string(exponent));

            return result;
        }

        /**
         * @brief 转换为分数形式的字符串
         * @param base 进制
         * @return 分数形式的字符串 pair<分子, 分母>
         */
        inline std::pair<std::string, std::string> to_string_fraction(const int &base = 10) const
        {
            std::pair<std::string, std::string> result;
            result.first = numerator_.to_string(base);
            result.second = denominator_.to_string(base);
            return result;
        }

        /**
         * @brief 获取分子（常量版本）
         * @return 分子的常量引用
         */
        inline const big_uint &numerator() const
        {
            return numerator_;
        }

        /**
         * @brief 获取分子（可修改版本）
         * @return 分子的引用
         */
        inline big_uint &numerator()
        {
            return numerator_;
        }

        /**
         * @brief 获取分母（常量版本）
         * @return 分母的常量引用
         */
        inline const big_uint &denominator() const
        {
            return denominator_;
        }

        /**
         * @brief 获取分母（可修改版本）
         * @return 分母的引用
         */
        inline big_uint &denominator()
        {
            return denominator_;
        }

        /**
         * @brief 获取是否为负数
         * @return 是否为负数
         */
        inline bool is_negative() const
        {
            return is_negative_;
        }

        /**
         * @brief 比较 a == b
         * @param other 另一个分数
         * @return 是否 a == b
         */
        inline bool operator==(const fraction &other) const
        {
            return is_negative_ == other.is_negative_ && numerator_ == other.numerator_ && denominator_ == other.denominator_;
        }

        /**
         * @brief 比较 a != b
         * @param other 另一个分数
         * @return 是否 a != b
         */
        inline bool operator!=(const fraction &other) const
        {
            return !(*this == other);
        }

        /**
         * @brief 比较 a < b
         * @param other 另一个分数
         * @return 是否 a < b
         */
        inline bool operator<(const fraction &other) const
        {
            if (!is_negative_ and !other.is_negative_)
                return numerator_ * other.denominator_ < other.numerator_ * denominator_;
            if (!is_negative_ and other.is_negative_)
                return false;
            if (is_negative_ and !other.is_negative_)
                return true;
            if (is_negative_ and other.is_negative_)
                return numerator_ * other.denominator_ > other.numerator_ * denominator_;
            return false;
        }

        /**
         * @brief 比较 a > b
         * @param other 另一个分数
         * @return 是否 a > b
         */
        inline bool operator>(const fraction &other) const
        {
            return other < *this;
        }

        /**
         * @brief 比较 a <= b
         * @param other 另一个分数
         * @return 是否 a <= b
         */
        inline bool operator<=(const fraction &other) const
        {
            return !(*this < other);
        }

        /**
         * @brief 比较 a >= b
         * @param other 另一个分数
         * @return 是否 a >= b
         */
        inline bool operator>=(const fraction &other) const
        {
            return !(*this < other);
        }

        /**
         * @brief 加法赋值运算符
         * @param value 加的分数
         * @return 自身引用
         */
        inline fraction &operator+=(const fraction &value)
        {
            max_bits_ = std::max(max_bits_, value.max_bits_);
            // 处理符号：如果两个分数符号不同，则转换为减法
            if (is_negative_ != value.is_negative_)
            {
                // 保存当前符号
                bool original_sign = is_negative_;
                // 临时改变value的符号进行减法运算
                is_negative_ = value.is_negative_;
                *this -= value;
                // 恢复当前分数的原始符号状态
                is_negative_ = !is_negative_;
                return *this;
            }

            // 同号分数相加
            // a/b + c/d = (a*d + c*b) / (b*d)
            big_uint new_numerator = numerator_ * value.denominator_ + value.numerator_ * denominator_;
            big_uint new_denominator = denominator_ * value.denominator_;

            numerator_ = std::move(new_numerator);
            denominator_ = std::move(new_denominator);

            // 符号保持不变（同号相加）

            // 化简结果
            simplify();

            return *this;
        }

        /**
         * @brief 加法运算符
         * @param value 加的分数
         * @return 新对象
         */
        inline fraction operator+(const fraction &value) const
        {
            return fraction(*this) += value;
        }

        /**
         * @brief 减法赋值运算符
         * @param value 减的分数
         * @return 自身引用
         */
        inline fraction &operator-=(const fraction &value)
        {
            max_bits_ = std::max(max_bits_, value.max_bits_);
            // 处理符号：如果两个分数符号不同，则转换为加法
            if (is_negative_ != value.is_negative_)
            {
                // 保存当前符号
                bool original_sign = is_negative_;
                // 临时改变value的符号进行加法运算
                is_negative_ = value.is_negative_;
                *this += value;
                // 恢复当前分数的原始符号状态
                is_negative_ = !is_negative_;
                return *this;
            }

            // 同号分数相减
            // a/b - c/d = (a*d - c*b) / (b*d)
            big_uint new_numerator, new_denominator;

            // 计算交叉乘积
            big_uint cross1 = numerator_ * value.denominator_;
            big_uint cross2 = value.numerator_ * denominator_;

            // 判断大小以确定结果符号
            if (cross1 >= cross2)
            {
                new_numerator = cross1 - cross2;
                // 结果符号与第一个操作数相同，无需改变is_negative_
            }
            else
            {
                new_numerator = cross2 - cross1;
                // 结果符号与第二个操作数相反
                is_negative_ = !is_negative_;
            }

            new_denominator = denominator_ * value.denominator_;

            numerator_ = std::move(new_numerator);
            denominator_ = std::move(new_denominator);

            // 化简结果
            simplify();

            return *this;
        }

        /**
         * @brief 减法运算符
         * @param value 减的分数
         * @return 新对象
         */
        inline fraction operator-(const fraction &value) const
        {
            return fraction(*this) -= value;
        }

        /**
         * @brief 乘法赋值运算符
         * @param value 乘的分数
         * @return 自身引用
         */
        inline fraction &operator*=(const fraction &value)
        { 
            // 符号计算
            is_negative_ ^= value.is_negative_;
            max_bits_ = std::max(max_bits_, value.max_bits_);

            numerator_ *= value.numerator_;
            denominator_ *= value.denominator_; 
            simplify();
            return *this;
        }

        /**
         * @brief 乘法运算符
         * @param value 乘的分数
         * @return 新对象
         */
        inline fraction operator*(const fraction &value) const
        {
            return fraction(*this) *= value;
        }

        /**
         * @brief 除法赋值运算符
         * @param value 除的分数
         * @return 自身引用
         */
        inline fraction &operator/=(const fraction &value)
        { 
            is_negative_ ^= value.is_negative_;
            max_bits_ = std::max(max_bits_, value.max_bits_);

            numerator_ *= value.denominator_;
            denominator_ *= value.numerator_; 

            simplify();
            return *this;
        }

        /**
         * @brief 除法运算符
         * @param value 除的分数
         * @return 新对象
         */
        inline fraction operator/(const fraction &value) const
        {
            return fraction(*this) /= value;
        }

        /**
         * @brief 牛顿迭代法求平方根
         * @param
         * @param precision 精度要求（小数点后位数, 二进制位数）
         * @return 平方根结果
         */
        inline static fraction sqrt(const fraction &value, const uint64_t &precision = 0)
        {
            const uint64_t new_max_bits = std::max(value.max_bits_, precision) + 32;

            if (value.is_negative_)
            {
                throw std::runtime_error("chenc::big_int::fraction.sqrt Square root of negative fraction is not defined.");
            }

            if (value.numerator_.is_zero())
            {
                return fraction(0, 1, std::max(value.max_bits_, precision));
            }

            // 智能初始猜测
            fraction x(1, 1, new_max_bits);
            // 使用对数来估算平方根
            // log(sqrt(x)) = log(x)/2
            // sqrt(x) = 2^(log2(x)/2)

            // 获取分子分母的位数
            uint64_t num_bits = value.numerator_.bits();
            uint64_t den_bits = value.denominator_.bits();

            // 估算指数
            int64_t total_bits = static_cast<int64_t>(num_bits) - static_cast<int64_t>(den_bits);

            if (total_bits == 0)
            {
                return fraction(1, 1, std::max(value.max_bits_, precision)); // 大约是1
            }

            // sqrt(2^n) = 2^(n/2)
            int64_t sqrt_bits = total_bits / 2;

            if (sqrt_bits >= 0)
            {
                // 构造 2^(sqrt_bits)
                big_uint result(1);
                result <<= sqrt_bits;
                x.numerator_ = result;
            }
            else
            {
                // 构造 1/2^(-sqrt_bits)
                big_uint result(1);
                result <<= (-sqrt_bits);
                x.denominator_ = result;
            }

            // 牛顿迭代，带自适应停止条件
            const fraction half(1, 2, new_max_bits);
            fraction tolerance(1, big_uint::pow(2, std::min(value.max_bits_, precision)) + 4, new_max_bits);

            for (int i = 0;; i++)
            { // 限制迭代次数
                if (i >= 100)
                    throw std::runtime_error("chenc::big_int::fraction.sqrt iteration count exceeded");
                fraction x_prev = x;
                auto temp = value;
                temp.max_bits_ = new_max_bits;
                x = half * (x + (temp /= x));

                // 检查收敛
                fraction error = x > x_prev ? x - x_prev : x_prev - x;
                if (error < tolerance)
                    break;
            }
            x.max_bits_ = std::max(value.max_bits_, precision);
            return x;
        }

        /**
         * @brief pow 幂次函数 a^b
         * @param a 底数
         * @param b 幂次
         * @return 结果
         */
        inline static fraction pow(const fraction &a, const fraction &b)
        {
            const uint64_t precision = std::max(a.max_bits_, b.max_bits_);
            // --- 1. 处理边界情况 ---
            if (a.numerator_.is_zero())
            {
                if (b.is_negative_ || b.numerator_.is_zero())
                    throw std::domain_error("chenc::big_int::fraction.pow 0^b is undefined for b <= 0");
                return fraction(0, 1, precision); // 0^b = 0 for b > 0
            }
            if (a.is_one())
                if (a.is_negative_)
                    return fraction(-1, 1, precision); // -1^b = -1
                else
                    return fraction(1, 1, precision); // 1^b = 1
            if (b.is_zero())
                return fraction(1, 1, precision); // a^0 = 1
            if (b.is_one())
                return fraction(a).set_precision(precision); // a^1 = a

            if (a.is_negative_)
            {
                // 如果底数为负，只有当指数的分母为奇数时，在实数域上有意义
                if (!b.denominator_.bit_test(0))
                {
                    throw std::domain_error("chenc::big_int::fraction.pow Negative base raised to a power with an even denominator is not a real number.");
                }
            }

            // --- 2. 指数 b 是整数的快速路径 ---
            if (b.denominator_.is_one())
            {
                fraction result = pow_integer_exponent(a, b.numerator_);
                if (b.is_negative_)
                {
                    if (result.numerator_.is_zero())
                    {
                        throw division_by_zero("chenc::big_int::fraction.pow Result of negative power is division by zero.");
                    }
                    // a^(-n) = 1 / a^n
                    std::swap(result.numerator_, result.denominator_);
                }
                return result;
            }

            // --- 3. 指数 b 是分数的通用路径: a^b = exp(b * log(a)) ---

            // 计算 log(a)
            fraction log_a = log(a, precision);

            // 计算 b * log(a)
            fraction b_log_a = b * log_a;

            // 计算 exp(...)
            return exp(b_log_a, precision);
        }
        /**
         * @brief 计算以任意分数为底的对数 log_base(x)
         * @param base 对数的底数
         * @param x 输入值
         * @param precision 精度要求（小数点后位数, 二进制位数）
         * @return log_base(x) 的分数表示
         */
        inline static fraction log(const fraction &base, const fraction &x, const uint64_t &precision = UINT64_MAX)
        {
            // --- 检查定义域 ---
            if (base.is_negative_ || base.numerator_.is_zero() || base.is_one())
            {
                throw std::domain_error("chenc::big_int::fraction.log Logarithm base must be positive and not equal to 1.");
            }
            if (x.is_negative_ || x.numerator_.is_zero())
            {
                throw std::domain_error("chenc::big_int::fraction.log Logarithm value must be positive.");
            }

            // --- 使用换底公式: log_base(x) = ln(x) / ln(base) ---
            const uint64_t pre = std::min(std::max(base.max_bits_, x.max_bits_), precision);

            fraction ln_x = natural_log(x, pre);
            fraction ln_base = natural_log(base, pre);

            if (ln_base.numerator_.is_zero())
            {
                // 理论上只有当 base=1 时才会发生，前面已拦截，但作为安全检查
                throw division_by_zero("chenc::big_int::fraction.log Logarithm base evaluates to 1, causing division by zero.");
            }

            return ln_x / ln_base;
        }

        inline friend std::ostream &operator<<(std::ostream &os, const fraction &value)
        {
            os << value.to_string();
            return os;
        }
        inline friend std::istream &operator>>(std::istream &is, fraction &value)
        {
            std::string str;
            is >> str;
            value = fraction(str);
            return is;
        }

    private:
        /**
         * @brief [辅助函数] 计算自然对数 log(x)，使用泰勒级数展开
         * @note 使用 log((1+y)/(1-y)) 的级数，收敛速度更快
         * @param val 输入值 (必须为正)
         * @param precision 迭代精度控制
         * @return ln(val) 的近似值
         */
        inline static fraction natural_log(const fraction &val, const uint64_t &precision)
        {
            const uint64_t new_max_bits = std::max(val.max_bits_, precision) + 32;
            if (val.is_negative_ || val.numerator_.is_zero())
            {
                throw std::domain_error("chenc::big_int::fraction.natural_log Logarithm of non-positive number is undefined.");
            }
            if (val.is_one())
            {
                return fraction(0, 1, std::max(val.max_bits_, precision));
            }

            // 使用公式: log(x) = 2 * (y + y^3/3 + y^5/5 + ...), 其中 y = (x-1)/(x+1)
            fraction one(1, 1, new_max_bits);
            fraction two(2, 1, new_max_bits);
            fraction y = (val - one) / (val + one);

            fraction y_squared = y * y;
            fraction term = y;
            fraction sum = term;

            // 设置一个合理的容差来决定何时停止迭代
            fraction tolerance(1, big_uint(1) << (precision + 4), new_max_bits);

            for (uint64_t n = 3;; n += 2)
            {
                term *= y_squared;
                // 避免创建临时对象 fraction(n, 1, ...)
                fraction next_term = term;
                next_term.denominator_ *= n;

                // 检查收敛
                // 使用交叉相乘避免除法: a/b < c/d  -> a*d < b*c
                if (next_term.numerator_.is_zero() || (next_term.numerator_ * tolerance.denominator_ < tolerance.numerator_ * next_term.denominator_))
                {
                    break;
                }
                sum += next_term;
            }
            fraction result = two * sum;
            result.max_bits_ = std::max(val.max_bits_, precision);
            return result;
        }
        /**
         * @brief [辅助函数] 使用快速幂算法计算分数的整数次幂
         * @param base 底数
         * @param exp 指数
         * @return base^exp 的结果
         */
        inline static fraction pow_integer_exponent(fraction base, big_uint exp)
        {
            base.max_bits_ = std::max(base.max_bits_, exp.bits());
            fraction result(1, 1, base.max_bits_);
            while (!exp.is_zero())
            {
                if (exp.bit_test(0)) // 如果指数是奇数
                {
                    result *= base;
                }
                base *= base;
                exp >>= 1;
            }
            result.max_bits_ = std::max(result.max_bits_, base.max_bits_);
            return result;
        }

        /**
         * @brief [辅助函数] 计算 e^x，使用泰勒级数展开
         * @note exp(x) = 1 + x + x^2/2! + x^3/3! + ...
         * @param val 输入值
         * @param precision 迭代精度控制
         * @return exp(val) 的近似值
         */
        inline static fraction exp(const fraction &val, const uint64_t &precision)
        {
            const uint64_t new_max_bits = std::max(val.max_bits_, precision) + 32;
            fraction term(1, 1, new_max_bits);
            fraction sum = term;

            fraction tolerance(1, big_uint(1) << (precision + 1), new_max_bits);

            for (uint64_t n = 1;; ++n)
            {
                term = (term * val) / fraction(n, 1, new_max_bits);

                // 检查收敛
                if (term.numerator_.is_zero() || (term.numerator_ * tolerance.denominator_ < term.denominator_))
                {
                    break;
                }
                sum += term;
            }
            sum.max_bits_ = std::max(val.max_bits_, precision);
            return sum;
        }
        /**
         * @brief 化简分数
         * @note 使用最大公约数化简分子和分母
         */
        void simplify()
        { 
            if (numerator_.is_one() || denominator_.is_one())
            {
                return;
            }
            if (denominator_ == 0)
            {
                throw division_by_zero("chenc::big_int::fraction.simplify denominator_ is zero");
            }
            if (numerator_ == 0)
            {
                denominator_ = 1;
                is_negative_ = false;
                return;
            }
            if (numerator_ == denominator_)
            {
                numerator_ = 1;
                denominator_ = 1;
                return;
            }

            // 精度超限
            uint64_t numerator_bits = numerator_.bits();     // 分子
            uint64_t denominator_bits = denominator_.bits(); // 分母
            if (denominator_bits > numerator_bits)
            {
                if (numerator_bits > max_bits_)
                {
                    numerator_ >>= (numerator_bits - max_bits_);
                    denominator_ >>= (numerator_bits - max_bits_);
                }
            }
            else
            {
                if (denominator_bits > max_bits_)
                {
                    numerator_ >>= (denominator_bits - max_bits_);
                    denominator_ >>= (denominator_bits - max_bits_);
                }
            }

            // 使用 big_uint 的 GCD 方法化简
            big_uint gcd = big_uint::gcd(numerator_, denominator_);
            // std::cout << "gcd: " << gcd << std::endl;
            if (gcd > 1)
            {
                numerator_ = numerator_ / gcd;
                denominator_ = denominator_ / gcd;
            }
 
        }

        big_uint numerator_;   // 分子
        big_uint denominator_; // 分母
        bool is_negative_;     // 是否为负数
        uint64_t max_bits_;    // 最大有效精度
    };
}

#endif