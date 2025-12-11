#ifndef DYNAMICARRAY_HPP
#define DYNAMICARRAY_HPP

#include <type_traits>
#include <stdexcept>
#include <initializer_list>
#include <concepts>

// CURRENT VERSION v0.1.0


                                         /* ПОДМЕНА АЛЛОКАТОРА НЕ ТЕСТИРОВАЛАСЬ */
namespace mystl {         //                            /
                          //                          |/_
template<typename T, typename Allocator = std::allocator<T>>
class DynamicArray {
private:

    T* arr;

    size_t sz;
    size_t cap;

    Allocator alloc;

    using AllocatorTraits = std::allocator_traits<Allocator>;

    //COMMON ITERATOR BLOCK

    template <bool IsConst>
    class common_iterator {
    private:

        using ConditionalPtr = std::conditional_t<IsConst, const T*, T*>;
        using ConditionalRef = std::conditional_t<IsConst, const T&, T&>;
        using ConditionalType = std::conditional_t<IsConst, const T, T>;

        ConditionalPtr ptr;

    public:

        using value_type        = ConditionalType;
        using difference_type   = std::ptrdiff_t;
        using reference         = ConditionalRef;
        using pointer           = ConditionalPtr;
        using iterator_category = std::random_access_iterator_tag; // не знаю, как сделать contiguous

        /**
         * @brief Преобразование обычного итератора в константный
         *
         * @param other - неконстантный итератор
         */
        common_iterator(const common_iterator<false>& other)
        requires IsConst
        : ptr(other.base()) {}

        /**
         * @brief Конструктор из сырого указателя
         *
         * @param p - std::conditional_t<IsConst, const T*, T*>
         */
        common_iterator(ConditionalPtr p) : ptr(p) {}

        common_iterator() = default;
        ~common_iterator() = default;

        ConditionalRef operator * () const { return *ptr; }                                          // Input
        ConditionalPtr operator -> () const { return ptr; }                                          // iterator
        template <bool OtherConst>                                                                   // properties
        bool operator == (const common_iterator<OtherConst>& other) const {return ptr == other.ptr;} //
        template <bool OtherConst>                                                                   //
        bool operator != (const common_iterator<OtherConst>& other) const {return ptr != other.ptr;} //
        common_iterator<IsConst>& operator ++ () { ++ptr; return *this; }                            //
        common_iterator<IsConst> operator ++ (int) {                                                 //
            common_iterator<IsConst> copy = *this;                                                   //
            ++(*this);                                                                               //
            return copy;                                                                             //
        }                                                                                            //

        common_iterator(const common_iterator& other) = default;                                     // Forward
        common_iterator& operator=(const common_iterator& other) = default;                          // iterator
                                                                                                     // properties

        common_iterator<IsConst>& operator -- () { --ptr; return *this; }                            // Bidirectional
        common_iterator<IsConst> operator -- (int) {                                                 // iterator
            common_iterator<IsConst> copy = *this;                                                   // properties
            --(*this);                                                                               //
            return copy;                                                                             //
        };                                                                                           //

        common_iterator<IsConst> operator + (difference_type n) { return common_iterator(ptr + n); } // Random
        common_iterator<IsConst> operator - (difference_type n) { return common_iterator(ptr - n); } // access
        common_iterator<IsConst>& operator += (difference_type n) { ptr += n; return *this; }        // iterator
        common_iterator<IsConst>& operator -= (difference_type n) { ptr -= n; return *this; }        // properties
        bool operator < (const common_iterator<IsConst>& other) const { return ptr < other.ptr; }    //
        bool operator > (const common_iterator<IsConst>& other) const { return ptr > other.ptr; }    //
        bool operator <= (const common_iterator<IsConst>& other) const { return ptr <= other.ptr; }  //
        bool operator >= (const common_iterator<IsConst>& other) const { return ptr >= other.ptr; }  //
        difference_type operator - (const common_iterator<IsConst>& other) const                     //
        { return ptr - other.ptr; }                                                                  //
        ConditionalRef operator [] (difference_type n) const { return ptr[n]; }                      //

        /**
         * @brief Геттер, возвращающий сырой указатель
         *
         * @return std::conditional_t<IsConst, const T*, T*>
         */
        ConditionalPtr base() const noexcept { return ptr; }
    };

public:

    //ORDINARY ITERATOR BLOCK

    using iterator = common_iterator<false>;
    using const_iterator = common_iterator<true>;

    iterator begin() { return iterator(arr); }
    const_iterator begin() const { return const_iterator(arr); }

    iterator end() { return iterator(arr + sz); }
    const_iterator end() const { return const_iterator(arr + sz); }

    //REVERSED ITERATOR BLOCK

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }

    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

    //BASIC FUNCTIONAL BLOCK

    /**
     * @brief Конструктор по умолчанию. Создает пустой DynamicArray.
     *
     * @exception Не бросает исключений
     */
    DynamicArray() noexcept : arr(nullptr), sz(0), cap(0), alloc(Allocator()) {}

    /**
     * @brief Конструктор с заданным размером и значением (для копируемых типов).
     *
     * @param size Начальный размер массива
     * @param value Значение для инициализации элементов
     * @param alloc Аллокатор для управления памятью
     *
     * @exception std::bad_alloc При невозможности выделить память
     * @exception Любые исключения от конструктора копирования T
     */
    explicit DynamicArray(size_t size, const T& value = T(), Allocator alloc = Allocator())
    requires std::copy_constructible<T>
        : arr(nullptr), sz(0), cap(0), alloc(alloc)
    {
        reserve(size);
        sz = size;

        size_t i = 0;
        try {
            for(i = 0; i < sz; ++i) AllocatorTraits::construct(alloc, arr + i, value);
        } catch (const std::exception&) {
            for (size_t j = 0; j < i; ++j) AllocatorTraits::destroy(alloc, arr + j);
            AllocatorTraits::deallocate(alloc, arr, cap);
            throw;
        }
    }

    /**
     * @brief Конструктор с заданным размером (для некопируемых типов).
     *
     * @param size Начальный размер массива
     * @param alloc Аллокатор для управления памятью
     *
     * @exception std::bad_alloc При невозможности выделить память
     * @exception Любые исключения от дефолтного конструктора T
     */
    explicit DynamicArray(size_t size, Allocator alloc = Allocator())
    requires (!std::copy_constructible<T> && std::default_initializable<T>)
        : arr(nullptr), sz(0), cap(0), alloc(alloc)
    {
        reserve(size);
        sz = size;

        size_t i = 0;
        try {
            for(i = 0; i < sz; ++i) AllocatorTraits::construct(alloc, arr + i);
        } catch (const std::exception&) {
            for (size_t j = 0; j < i; ++j) AllocatorTraits::destroy(alloc, arr + j);
            AllocatorTraits::deallocate(alloc, arr, cap);
            throw;
        }
    }

    /**
     * @brief Конструктор из списка инициализации.
     *
     * @param list
     *
     * @exception std::bad_alloc При невозможности выделить память
     * @exception Любые исключения от конструктора копирования T
     */
    DynamicArray(std::initializer_list<T> list)
    requires std::copy_constructible<T>
        : arr(nullptr), sz(0), cap(0), alloc(Allocator())
    {
        reserve(list.size());
        sz = list.size();
        size_t i = 0;
        try {
            for (const T& value : list) {
                AllocatorTraits::construct(alloc, arr + i, value);
                i++;
            }
        } catch (const std::exception&) {
            for (size_t j = 0; j < i; ++j) AllocatorTraits::destroy(alloc, arr + j);
            AllocatorTraits::deallocate(alloc, arr, cap);
            throw;
        }
    }

    /**
     * @brief Деструктор.
     *
     * @exception Не бросает исключений
     */
    ~DynamicArray() {
        if (arr != nullptr) {
            for (size_t i = 0; i < sz; ++i) AllocatorTraits::destroy(alloc, arr + i);
            AllocatorTraits::deallocate(alloc, arr, cap);
        }
    }

    /**
     * @brief Конструктор копирования
     * @param d_arr Другой DynamicArray
     *
     * @exception std::bad_alloc При невозможности выделить память
     * @exception Любые исключения от конструктора копирования T
     */
    DynamicArray(const DynamicArray& d_arr)
    requires std::copy_constructible<T>
        : arr(nullptr), sz(0), cap(0), alloc(std::allocator_traits<Allocator>::select_on_container_copy_construction(d_arr.alloc))
    {
        reserve(d_arr.cap);
        sz = d_arr.sz;

        size_t i;
        try{
            for(i = 0; i < sz; ++i) AllocatorTraits::construct(alloc, arr + i, d_arr.arr[i]);
        }
        catch(const std::exception&) {
            for(size_t j = 0; j < i; ++j) AllocatorTraits::destroy(alloc, arr + j);
            AllocatorTraits::deallocate(alloc, arr, cap);
            throw;
        }
    }

    /**
     * @brief Оператор присваивания копированием.
     *
     * @param d_arr Другой DynamicArray
     * @return DynamicArray& Ссылка на этот массив
     *
     * @exception std::bad_alloc При невозможности выделить память
     * @exception Любые исключения от конструктора копирования T
     */
    DynamicArray& operator = (const DynamicArray& d_arr)
    requires std::copyable<T>
    {
        if (this != &d_arr) {
            DynamicArray temp(d_arr);
            *this = std::move(temp);
        }
        return *this;
    }

    /**
     * @brief Конструктор перемещения
     *
     * @param d_arr Другой DynamicArray
     *
     * @exception Не бросает исключений
     */
    DynamicArray(DynamicArray&& d_arr) noexcept
        : arr(d_arr.arr), sz(d_arr.sz), cap(d_arr.cap), alloc(std::move(d_arr.alloc))
    {
        d_arr.sz = 0;
        d_arr.cap = 0;
        d_arr.arr = nullptr;
        d_arr.alloc = Allocator();
    }

    /**
     * @brief Оператор присваивания перемещением.
     *
     * @param d_arr Другой DynamicArray
     * @return DynamicArray& Ссылка на этот массив
     *
     * @exception Не бросает исключений
     */
    DynamicArray& operator = (DynamicArray&& d_arr) noexcept
    {
        if (this != &d_arr) {
            if(arr != nullptr){                                                                  //
                for(size_t i = 0; i < sz; ++i) AllocatorTraits::destroy(alloc, arr + i);         // old array destruction
                AllocatorTraits::deallocate(alloc, arr, cap);                                    //
                arr = nullptr;                                                                   //
            }                                                                                    //

            sz = d_arr.sz;
            cap = d_arr.cap;
            arr = d_arr.arr;
            alloc = std::move(d_arr.alloc);

            d_arr.sz = 0;
            d_arr.cap = 0;
            d_arr.arr = nullptr;
            d_arr.alloc = Allocator();
        }
        return *this;
    }

    //RESERVE and SHRINK_TO_FIT BLOCK

    /**
     * @brief Увеличивает ёмкость массива до указанного размера.
     *
     * @param n Новая минимальная ёмкость
     *
     * @exception std::bad_alloc При невозможности выделить память
     * @exception Любые исключения от конструкторов перемещения/копирования T
     */
    void reserve(size_t n) {
        if (n <= cap) return;

        T* newarr = AllocatorTraits::allocate(alloc, n);

        size_t i = 0;
        try{
            for (; i < sz; ++i) AllocatorTraits::construct(alloc, newarr + i, std::move_if_noexcept(arr[i]));
        }
        catch(const std::exception&) {
            for (size_t j = 0; j < i; ++j) AllocatorTraits::destroy(alloc, newarr + j);
            AllocatorTraits::deallocate(alloc, newarr, n);
            throw;
        }

        for(size_t k = 0; k < sz; ++k) AllocatorTraits::destroy(alloc, arr + k);
        AllocatorTraits::deallocate(alloc, arr, cap);

        arr = newarr;
        cap = n;
    }
    /**
    * @brief Уменьшает ёмкость массива до размера.
    *
    * @exception std::bad_alloc При невозможности выделить память
    * @exception Любые исключения от конструкторов перемещения/копирования T
    */
    void shrink_to_fit() {
        if (sz == cap) return;

        T* newarr = AllocatorTraits::allocate(alloc, sz);

        size_t i = 0;
        try{
            for (; i < sz; ++i) AllocatorTraits::construct(alloc, newarr + i, std::move_if_noexcept(arr[i]));
        }
        catch(const std::exception&) {
            for (size_t j = 0; j < i; ++j) AllocatorTraits::destroy(alloc, newarr + j);
            AllocatorTraits::deallocate(alloc, newarr, sz);
            throw;
        }
        for(size_t g = 0; g < sz; ++g) AllocatorTraits::destroy(alloc, arr + g);
        AllocatorTraits::deallocate(alloc, arr, cap);

        arr = newarr;
        cap = sz;
    }

    //PUSH_BACK BLOCK

    /**
    * @brief Создает элемент на месте в конце массива.
    *
    * @tparam Args Типы аргументов для конструктора T
    * @param args Аргументы для передачи конструктору T
    *
    * @exception std::bad_alloc При необходимости увеличения capacity и невозможности выделить память
    * @exception Любые исключения от конструктора T
    */
    template <typename... Args>
    requires std::constructible_from<T, Args...>
    void emplace_back(Args&&... args) {
        if (cap == sz) reserve(sz > 0 ? 2 * sz : 1);
        AllocatorTraits::construct(alloc, arr + sz, std::forward<Args>(args)...);
        ++sz;
    }

    /**
    * @brief Добавляет элемент в конец массива (копирование).
    *
    * @param value Элемент для копирования
    *
    * @exception std::bad_alloc При необходимости увеличения capacity
    * @exception Любые исключения от конструктора копирования T
    */
    void push_back(const T& value)
    requires std::copy_constructible<T>
    {
        emplace_back(value);
    }

    /**
    * @brief Добавляет элемент в конец массива (перемещение).
    *
    * @param value Элемент для перемещения
    *
    * @exception std::bad_alloc При необходимости увеличения capacity
    * @exception Любые исключения от конструктора перемещения T
    */
    void push_back(T&& value)
    requires std::movable<T>
    {
        emplace_back(std::move(value));
    }

    //ERASE BLOCK

private:

    /**
     * @brief Внутренняя реализация удаления диапазона [beg, end).
     *
     * @param beg Указатель на начало удаляемого диапазона
     * @param end Указатель на конец удаляемого диапазона
     * @return T* Указатель на позицию после удаления
     *
     * @exception Может бросать исключения от оператора копирования T
     */
    T* do_erase(T* beg, T* end) {
        size_t diff = end - beg;

        if (diff == 0) return end;

        for (T* p = beg; p + diff < arr + sz; ++p)
            *p = std::move_if_noexcept(*(p + diff));

        for (size_t i = 0; i < diff; ++i)
            AllocatorTraits::destroy(alloc, arr + sz - 1 - i);

        sz -= diff;
        return beg;
    }

public:

    /**
     * @brief Удаляет элемент в указанной позиции.
     *
     * @param pos Итератор на удаляемый элемент
     * @return iterator Итератор на элемент после удаленного
     *
     * @exception Может бросать исключения от оператора копирования T
     */
    iterator erase(iterator pos) { return iterator(do_erase(pos.base(), pos.base() + 1)); }

    /**
     * @brief Удаляет диапазон элементов [first, last).
     *
     * @param first Итератор на начало диапазона
     * @param last Итератор на конец диапазона
     * @return iterator Итератор на элемент после удаленного диапазона
     *
     * @exception Может бросать исключения от оператора копирования T
     */
    iterator erase(iterator first, iterator last) { return iterator(do_erase(first.base(), last.base())); }

    //RESIZE AND POP_BACK BLOCK

    /**
     * @brief Изменяет размер массива с инициализацией значением (для копируемых типов).
     *
     * @param count Новый размер массива
     * @param value Значение для инициализации новых элементов
     *
     * @exception std::bad_alloc При необходимости увеличения capacity
     * @exception Любые исключения от конструктора копирования T
     */
    void resize(size_t count, const T& value = T())
    requires std::copy_constructible<T>
    {
        if (count == sz) return;
        if (sz > count) {
            for (size_t i = count; i < sz; ++i) AllocatorTraits::destroy(alloc, arr + i);
            sz = count;
        } else if (sz < count) {
            if(count > cap) reserve(count);
            size_t i = sz;
            try{
                for (; i < count; ++i) AllocatorTraits::construct(alloc, arr + i, value);
            }catch(const std::exception&){
                for (size_t j = 0; j < i; ++j) AllocatorTraits::destroy(alloc, arr + j);
                throw;
            }
            sz = count;
        }
    }

    /**
     * @brief Изменяет размер массива (для некопируемых типов).
     *
     * @param count Новый размер массива
     *
     * @exception std::bad_alloc При необходимости увеличения capacity
     * @exception Любые исключения от конструктора по умолчанию T
     */
    void resize(size_t count)
        requires (!std::copy_constructible<T> && std::default_initializable<T>)
    {
        if (count == sz) return;

        if (count < sz) {
            for (size_t i = count; i < sz; ++i) AllocatorTraits::destroy(alloc, arr + i);
            sz = count;
        } else {
            if (count > cap) reserve(count);
            size_t i = sz;
            try {
                for (; i < count; ++i) AllocatorTraits::construct(alloc, arr + i);
            } catch (const std::exception&) {
                for (size_t j = sz; j < i; ++j) AllocatorTraits::destroy(alloc, arr + j);
                throw;
            }
            sz = count;
        }
    }

    /**
    * @brief Удаляет последний элемент массива.
    *
    * @exception Не бросает исключений
    */
    void pop_back() noexcept {
        if(sz == 0) return;
        --sz;
        AllocatorTraits::destroy(alloc, arr + sz);
    }

    //INSERTION BLOCK

    /**
    * @brief Создает элемент на месте в указанной позиции.
    *
    * @tparam Args Типы аргументов для конструктора T
    * @param pos Итератор указывающий на позицию для вставки
    * @param args Аргументы для передачи конструктору T
    * @return iterator Итератор на вставленный элемент
    *
    * @exception std::bad_alloc При необходимости увеличения capacity
    * @exception Любые исключения от конструктора T или перемещения элементов
    */
    template<typename... Args>
    requires std::constructible_from<T, Args...>
    iterator emplace(const_iterator pos, Args&&... args) {
        size_t insertion_id = pos - begin();

        if (cap == sz) reserve(sz > 0 ? 2 * sz : 1);
        ++sz;

        try{
            for (size_t i = sz - 1; i > insertion_id; --i) arr[i] = std::move_if_noexcept(arr[i - 1]);

            AllocatorTraits::construct(alloc, arr + insertion_id, std::forward<Args>(args)...);
        }
        catch(...) {
            for (size_t i = sz - 1; i > insertion_id; --i) arr[i] = std::move_if_noexcept(arr[i - 1]);
            --sz;
            throw;
        }
        return iterator(arr + insertion_id);
    }

    /**
     * @brief Вставляет элемент в указанную позицию (копирование).
     *
     * @param pos Итератор указывающий на позицию для вставки
     * @param value Элемент для копирования
     * @return iterator Итератор на вставленный элемент
     *
     * @exception std::bad_alloc При необходимости увеличения capacity
     * @exception Любые исключения от конструктора копирования T
     */
    iterator insert(const_iterator pos, const T& value)
    requires std::copy_constructible<T> { return emplace(pos, value); }

    /**
    * @brief Вставляет элемент в указанную позицию (перемещение).
    *
    * @param pos Итератор указывающий на позицию для вставки
    * @param value Элемент для перемещения
    * @return iterator Итератор на вставленный элемент
    *
    * @exception std::bad_alloc При необходимости увеличения capacity
    * @exception Любые исключения от конструктора перемещения T
    */
    iterator insert(const_iterator pos, T&& value)
    requires std::movable<T> { return emplace(pos, std::move(value)); }

    //ETC BLOCK
    /**
     * @brief Возвращает размер массива.
     *
     * @return size_t Количество элементов в массиве
     *
     * @exception Не бросает исключений
     */
    [[nodiscard]] size_t size() const noexcept { return sz; }

    /**
     * @brief Возвращает текущую ёмкость массива.
     *
     * @return size_t Текущая ёмкость массива
     *
     * @exception Не бросает исключений
     */
    [[nodiscard]] size_t capacity() const noexcept { return cap; }

    /**
     * @brief Проверяет пуст ли массив.
     *
     * @return true если массив пуст, false иначе
     *
     * @exception Не бросает исключений
     */
    [[nodiscard]] bool empty() const noexcept { return sz == 0; }

    /**
     * @brief Доступ к элементу по индексу без проверки границ.
     *
     * @param i Индекс элемента
     * @return T& Ссылка на элемент
     *
     * @exception Не бросает исключений
     */
    [[nodiscard]] T& operator[] (size_t i) noexcept { return arr[i]; }
    [[nodiscard]] const T& operator[] (size_t i) const noexcept { return arr[i]; }

    /**
     * @brief Возвращает ссылку на первый элемент массива.
     *
     * @return T& Ссылка на первый элемент
     *
     * @exception Не бросает исключений
     */
    [[nodiscard]] T& front() noexcept { return arr[0]; }
    [[nodiscard]] const T& front() const noexcept { return arr[0]; }

    /**
     * @brief Возвращает ссылку на последний элемент массива.
     *
     * @return T& Ссылка на последний элемент
     *
     * @exception Не бросает исключений
     */
    [[nodiscard]] T& back() noexcept { return arr[sz-1]; }
    [[nodiscard]] const T& back() const noexcept { return arr[sz-1]; }

    /**
     * @brief Возвращает указатель на данные массива.
     *
     * @return T* Указатель на данные массива
     *
     * @exception Не бросает исключений
     */
    [[nodiscard]] T* data() noexcept { return arr; }
    [[nodiscard]] const T* data() const noexcept { return arr; }

    /**
     * @brief Доступ к элементу по индексу с проверкой границ.
     *
     * @param i Индекс элемента
     * @return T& Ссылка на элемент
     *
     * @exception std::out_of_range Если индекс >= size()
     */
    [[nodiscard]]
    T& at(size_t i) {
        if (i >= sz) throw std::out_of_range("Index out of range");
        return arr[i];
    }

    [[nodiscard]]
    const T& at(size_t i) const {
        if (i >= sz) throw std::out_of_range("Index out of range");
        return arr[i];
    }

    /**
    * @brief Удаляет все элементы из массива.
    *
    * @exception Не бросает исключений
    */
    void clear() noexcept {
        for(size_t i = 0; i < sz; ++i) AllocatorTraits::destroy(alloc, arr + i);
        sz = 0;
    }

    /**
    * @brief Обменивает содержимое двух массивов.
    *
    * @param other Массив для обмена
    *
    * @exception Не бросает исключений
    */
    void swap(DynamicArray& other) noexcept{
        if(this == &other) return;
        std::swap(arr, other.arr);
        std::swap(sz, other.sz);
        std::swap(cap, other.cap);
    }
};

} // namespace mystl

#endif // DYNAMICARRAY_HPP
