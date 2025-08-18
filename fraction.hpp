#ifndef CHENC_FRACTION_HPP
#define CHENC_FRACTION_HPP

#include "big_uint.hpp"

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
        inline fraction()
            : numerator_(0), denominator_(1), is_negative_(false) {}

        /**
         * @brief 从分子和分母构造分数
         * @param numerator 分子
         * @param denominator 分母
         * @param max_bits 最大位数限制
         */
        inline fraction(const big_uint &numerator, const big_uint &denominator)
            : numerator_(numerator), denominator_(denominator), is_negative_(false) {}

        /**
         * @brief 从整数类型构造分数
         * @tparam T1 分子类型（整数）
         * @tparam T2 分母类型（整数）
         * @param numerator 分子
         * @param denominator 分母
         * @param max_bits 最大位数限制
         */
        template <typename T1, typename T2>
            requires std::integral<T1> && (sizeof(T1) <= sizeof(uint64_t)) && std::integral<T2> && (sizeof(T2) <= sizeof(uint64_t))
        inline fraction(const T1 &numerator, const T2 &denominator)
            : numerator_(numerator), denominator_(denominator), is_negative_((numerator < 0) ^ (denominator < 0))
        {
            numerator_ = numerator;
            denominator_ = denominator;
        }

        /**
         * @brief 从浮点数构造分数（使用连分数算法）
         * @tparam T1 浮点数类型
         * @param value 浮点数值
         * @param max_bits 最大位数限制
         */
        template <typename T1>
            requires std::floating_point<T1>
        inline fraction(const T1 &value)
        { // 提取符号
            is_negative_ = value < 0;
            T1 abs_value = std::abs(value);

            // 分离整数和小数部分
            T1 int_part, frac_part;
            frac_part = std::modf(abs_value, &int_part);

            // 初始化分子和分母
            big_uint num = static_cast<uint64_t>(int_part); // 整数部分作为初始分子
            big_uint den = 1;                               // 初始分母为1

            // 处理小数部分（连分数算法）
            if (frac_part > std::numeric_limits<T1>::epsilon())
            {
                big_uint p0 = 0, p1 = 1, q0 = 1, q1 = 0; // 连分数迭代变量
                T1 x = frac_part;
                uint64_t iteration = 0;
                const uint64_t max_iterations = 100; // 限制最大迭代次数，防止无限循环

                while (x > std::numeric_limits<T1>::epsilon() && iteration < max_iterations)
                {
                    T1 recip = 1.0 / x;
                    big_uint a = static_cast<uint64_t>(recip); // 当前连分数项
                    x = recip - a;                             // 更新剩余小数部分

                    // 更新分子和分母
                    big_uint p2 = a * p1 + p0;
                    big_uint q2 = a * q1 + q0;

                    // 更新迭代变量
                    p0 = p1;
                    p1 = p2;
                    q0 = q1;
                    q1 = q2;

                    ++iteration;
                }

                // 合并整数部分和小数部分
                num = num * q1 + p1;
                den = q1;
            }

            // 存储结果
            numerator_ = num;
            denominator_ = den;

            // 化简分数
            simplify();
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
        inline fraction &operator=(const fraction &value) = default;

        /**
         * @brief 移动赋值函数
         */
        inline fraction &operator=(fraction &&value) = default;

        /**
         * @brief 转换为浮点数
         * @tparam T1 目标浮点数类型
         * @return 转换后的浮点数
         */
        template <typename T1>
            requires std::floating_point<T1>
        inline operator T1() const
        {
            return static_cast<T1>(static_cast<long double>(numerator_) / static_cast<long double>(denominator_));
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
         * @brief 加法赋值运算符
         * @param value 加的分数
         * @return 自身引用
         */
        inline fraction &operator+=(const fraction &value)
        {
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

    private:
        /**
         * @brief 化简分数
         * @note 使用最大公约数化简分子和分母
         */
        void simplify()
        {
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

            // 使用 big_uint 的 GCD 方法化简
            big_uint gcd = big_uint::gcd(numerator_, denominator_);
            if (gcd > 1)
            {
                numerator_ = numerator_ / gcd;
                denominator_ = denominator_ / gcd;
            }
        }

        big_uint numerator_;   // 分子
        big_uint denominator_; // 分母
        bool is_negative_;     // 是否为负数
    };
}

#endif