#ifndef MAP_HPP
#define MAP_HPP

#include <functional>
#include <stdexcept>

// CURRENT VERSION v0.1.1

// CHANGELOG:
// > Исправлена критическая ошибка алгоритма удаления
// > Добавлены конструкторы от диапазона и std::initializer_list

namespace mystl {

template<
    typename Key,//         -----------  ПОДМЕНА АЛЛОКАТОРА И КОМПАРАТОРА НЕ ТЕСТИРОВАЛАСЬ
    typename T,  //        \|/                         /
    typename Compare = std::less<Key>,  //           |/_
    typename Allocator = std::allocator<std::pair<const Key, T>>
    >
class Map {
private:

    using value_type = std::pair<const Key, T>;

    // Фиктивная нода, обеспечивающая работу итератора
    struct BaseNode {
        BaseNode* left_ = nullptr;
        BaseNode* right_ = nullptr;
        BaseNode* parent_ = nullptr;
        bool is_red_ = false;
    };

    // Узел дерева
    struct Node : BaseNode {
        value_type value_;

        template<class... Args>
        requires std::constructible_from<value_type, Args...>
        Node(Args&&... args) : value_(std::forward<Args>(args)...) {}
    };

    // Хриним мнимую ноду
    BaseNode* imaginary_;

    // По стандарту нужен размер
    std::size_t size_;

    // Аллокатор и компаратор
    Compare comp_;
    Allocator alloc_;

    // Аллокатор для обычных узлов
    using node_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    node_allocator node_alloc_;

    // Аллокатор для фиктивных узлов
    using base_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<BaseNode>;
    base_allocator base_alloc_;

    //                   ~~Схема реализации~~
    //  ___________________
    // |  нода черная, но  |
    // |    это неважно    |
    //  ----------|--------   ______
    //            ↓          /    |/_ <--- цикл
    //          [черный]imaginary(BaseNode) <--- .end() итератор
    //                  /             \
    // [must be black] /             [черный]
    //           ↓    /              nullptr <--- (тут всегда nullptr)
    //         [черный]root(Node)                  \
    //             /           \                   |
    //   [?]node_1(Node)      [?]node_2(Node)      |
    //        /      \          /       \          |
    //      ...     ...       ...       ...        |> Красно-черное дерево
    //      /                                      |
    //  [?]node_n(Node) <--- .begin() итератор     |
    //     /       \                               |
    //   [черный*] [черный*]                       |
    //   nullptr   nullptr                        /
    //
    //
    // *nullptr считаются черными nil-нодами

    template <bool IsConst>
    class common_iterator {
    private:

        using ConditionalPtr = std::conditional_t<IsConst, const value_type*, value_type*>;
        using ConditionalRef = std::conditional_t<IsConst, const value_type&, value_type&>;
        using ConditionalType = std::conditional_t<IsConst, const value_type, value_type>;

        //Внутренняя структора итератора задается указателями на узел дерева, а не указателями на value_type
        using ConditionalBaseNodePtr = std::conditional_t<IsConst, const BaseNode*, BaseNode*>;
        using ConditionalNodePtr = std::conditional_t<IsConst, const Node*, Node*>;

        ConditionalBaseNodePtr node_ptr_;

    public:

        using value_type        = ConditionalType;
        using difference_type   = std::ptrdiff_t;
        using reference         = ConditionalRef;
        using pointer           = ConditionalPtr;
        using iterator_category = std::bidirectional_iterator_tag;

        /**
         * @brief Преобразование обычного итератора в константный
         *
         * @param other - неконстантный итератор
         */
        common_iterator(const common_iterator<false>& other) noexcept
            requires IsConst : node_ptr_(other.node_ptr_) {}

        /**
         * @brief Дефолт конструктор
         */
        common_iterator() = default;

        /**
         * @brief common_iterator - конструктор копирования
         *
         * @param other - другой итератор
         */
        common_iterator(const common_iterator& other) = default;

        /**
         * @brief operator = - оператор копирующего присваивания
         *
         * @param other - другой итератор
         *
         * @return common_iterator& - ссылка на себя
         */
        common_iterator& operator = (const common_iterator& other) = default;

        /**
         * @brief Конструктор из указателя на BaseNode
         *
         * @param node_ptr - std::conditional_t<IsConst, const BaseNode*, BaseNode*>
         */
        common_iterator(ConditionalBaseNodePtr node_ptr) noexcept : node_ptr_(node_ptr) {}

        /**
         * @brief operator *
         *
         * @return std::conditional_t<IsConst, const std::pair<const Key, T>&, std::pair<const Key, T>&>;
         */
        ConditionalRef operator * () const noexcept { return static_cast<ConditionalNodePtr>(node_ptr_)->value_; }

        /**
         * @brief operator ->
         *
         * @return std::conditional_t<IsConst, const std::pair<const Key, T>*, std::pair<const Key, T>*>;
         */
        ConditionalPtr operator -> () const noexcept { return &(static_cast<ConditionalNodePtr>(node_ptr_)->value_); }

        /**
         * @brief operator ==
         *
         * @param other - другой итератор любой константности
         *
         * @return bool
         */
        template <bool OtherConst>
        bool operator == (const common_iterator<OtherConst>& other) const noexcept
        { return node_ptr_ == other.node_ptr_; }

        /**
         * @brief operator !=
         *
         * @param other - другой итератор любой константности
         *
         * @return bool
         */
        template <bool OtherConst>
        bool operator != (const common_iterator<OtherConst>& other) const noexcept
        { return node_ptr_ != other.node_ptr_; }

        /**
         * @brief operator ++ - префиксный инкремент (inorder обход)
         *
         * @return common_oterator&
         */
        common_iterator& operator ++ () noexcept {
            if(node_ptr_->right_) {
                node_ptr_ = node_ptr_->right_;
                while (node_ptr_->left_ != nullptr) node_ptr_ = node_ptr_->left_;
            }
            else{
                auto parent = node_ptr_->parent_;
                while (parent != nullptr && node_ptr_ == parent->right_) {
                    node_ptr_ = parent;
                    parent = parent->parent_;
                }
                node_ptr_ = parent;
            }
            return *this;
        }

        /**
         * @brief operator ++ - постфиксный инкремент
         *
         * @return common_iterator
         */
        common_iterator operator ++ (int) noexcept {
            auto copy = *this;
            ++(*this);
            return copy;
        }

        /**
         * @brief operator -- - префиксный декремент
         *
         * @return common_iterator&
         */
        common_iterator& operator -- () noexcept {
            if(node_ptr_->left_ != nullptr) {
                node_ptr_ = node_ptr_->left_;
                while (node_ptr_->right_ != nullptr) node_ptr_ = node_ptr_->right_;
            }
            else {
                auto parent = node_ptr_->parent_;
                while (parent != nullptr && node_ptr_ == parent->left_) {
                    node_ptr_ = parent;
                    parent = parent->parent_;
                }
                node_ptr_ = parent;
            }
            return *this;
        }

        /**
         * @brief operator -- (постфиксный декремент)
         *
         * @return common_iterator
         */
        common_iterator operator -- (int) noexcept {
            auto copy = *this;
            --(*this);
            return copy;
        };

        /**
         * @brief Геттер, возвращающий сырой указатель
         *
         * @return std::conditional_t<IsConst, const BaseNode*, BaseNode*>
         */
        ConditionalBaseNodePtr base() const noexcept { return node_ptr_; }
    };

public:

    //ORDINARY ITERATOR BLOCK

    using iterator = common_iterator<false>;

    using const_iterator = common_iterator<true>;

    /**
     * @brief begin - итератор на начало таблицы
     *
     * @return iterator - итератор на самый левый узел дерева
     */
    iterator begin() noexcept {
        if (size_ == 0) return end();
        BaseNode* leftmost = imaginary_->left_;
        while (leftmost->left_ != nullptr) {
            leftmost = leftmost->left_;
        }
        return iterator(leftmost);
    }
    /**
     * @brief begin - итератор на начало таблицы
     *
     * @return const_iterator - константный итератор на самый левый узел дерева
     */
    const_iterator begin() const noexcept {
        if (size_ == 0) return end();
        const BaseNode* leftmost = imaginary_->left_;
        while (leftmost->left_ != nullptr) {
            leftmost = leftmost->left_;
        }
        return const_iterator(leftmost);
    }
    /**
     * @brief cbegin - строго константный итератор на начало таблицы
     *
     * @return const_iterator - константный итератор на самый левый узел дерева
     */
    const_iterator cbegin() const noexcept { return begin(); }


    /**
     * @brief end - итератор на конец таблицы
     *
     * @return iterator - итератор на мнимую ноду
     */
    iterator end() noexcept { return iterator(imaginary_); }
    /**
     * @brief end - итератор на конец таблицы
     *
     * @return const_iterator - константный итератор на мнимую ноду
     */
    const_iterator end() const noexcept { return const_iterator(imaginary_); }
    /**
     * @brief cend - строго константный итератор на мнимый узел
     *
     * @return const_iterator - константный итератор на мнимую ноду
     */
    const_iterator cend() const noexcept { return end(); }

    //REVERSE ITERATOR BLOCK

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /**
     * @brief rbegin
     *
     * @return std::reverse_iterator<iterator>
     */
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    /**
     * @brief rbegin
     *
     * @return std::reverse_iterator<const_iterator>
     */
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    /**
     * @brief rbegin
     *
     * @return std::reverse_iterator<const_iterator>
     */
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }


    /**
     * @brief rend
     *
     * @return std::reverse_iterator<iterator>
     */
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    /**
     * @brief rend
     *
     * @return std::reverse_iterator<const_iterator>
     */
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    /**
     * @brief rend
     *
     * @return std::reverse_iterator<const_iterator>
     */
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    //BASIC FUNCTIONAL BLOCK

private:

    /**
     * @brief create_imaginary - создать мнимую ноду
     *
     * @exception std::bad_alloc в случае ошибки выделения памяти
     *
     * @return BaseNode* - указатель на созданную ноду
     */
    BaseNode* create_imaginary() {
        BaseNode* node = std::allocator_traits<base_allocator>::allocate(base_alloc_, 1);
        std::allocator_traits<base_allocator>::construct(base_alloc_, node);
        node->left_ = nullptr;
        node->right_ = nullptr;
        node->parent_ = node;
        return node;
    }

    /**
     * @brief destroy_imaginary - освободить память из под мнимой ноды
     *
     * @param node - указатель на мниную ноду
     */
    void destroy_imaginary(BaseNode* node) noexcept {
        if(node) {
            std::allocator_traits<base_allocator>::destroy(base_alloc_, node);
            std::allocator_traits<base_allocator>::deallocate(base_alloc_, node, 1);
        }
    }

    /**
     * @brief create_node - создать ноду из переданных аргументов
     *
     * @param args - кортеж аргументов
     *
     * @exception std::bad_alloc в случае ошибки выделения памяти
     *
     * @return Node* - указатель на созданную ноду
     */
    template <typename... Args>
    Node* create_node(Args&&... args)
        requires std::constructible_from<Node, Args...>
    {
        Node* node = std::allocator_traits<node_allocator>::allocate(node_alloc_, 1);
        try {
            std::allocator_traits<node_allocator>::construct(node_alloc_, node, std::forward<Args>(args)...);
        } catch (...) {
            std::allocator_traits<node_allocator>::deallocate(node_alloc_, node, 1);
            throw;
        }
        return node;
    }

    /**
     * @brief cleaner - рекурсивная очистка дерева(Не очищает память imaginary_)
     *
     * @param node - указатель на узел
     */
    void cleaner(BaseNode* node) noexcept {
        if (node == nullptr || node == imaginary_) return;

        cleaner(node->left_);
        cleaner(node->right_);

        if (node->parent_) {
            if (node->parent_->left_ == node)
                node->parent_->left_ = nullptr;
            else if (node->parent_->right_ == node)
                node->parent_->right_ = nullptr;
        }

        auto real = static_cast<Node*>(node);
        std::allocator_traits<node_allocator>::destroy(node_alloc_, real);
        std::allocator_traits<node_allocator>::deallocate(node_alloc_, real, 1);
    }

    /**
     * @brief cloner - клонирование дерева(Не клонирует imaginary_)
     *
     * @param node - указатель на клононируемое дерево
     * @param parent - указатель на родителя(для установления связей в новом дереве)
     *
     * @return Node* - указатель на склонированное дерево
     *
     * @exception Любые исключения от конструктора копирования Key, T
     */
    Node* cloner(BaseNode* node, BaseNode* parent) {
        if (node == nullptr) return nullptr;

        Node* real = static_cast<Node*>(node);
        auto new_node = create_node(real->value_);

        new_node->is_red_ = node->is_red_;
        new_node->parent_ = parent;
        new_node->left_ = cloner(node->left_, new_node);
        new_node->right_ = cloner(node->right_, new_node);

        return new_node;
    }

public:

    /**
     * @brief Деструктор
     *
     * @exception std::bad_alloc при невозможности выделения памяти
     */
    ~Map() {
        clear();
        destroy_imaginary(imaginary_);
    }

    /**
     * @brief Дефолт конструктор
     *
     * @exception std::bad_alloc при невозможности выделения памяти
     */
    Map() :
        imaginary_(create_imaginary()),
        size_(0),
        comp_(Compare()),
        alloc_(Allocator()),
        node_alloc_(Allocator()),
        base_alloc_(Allocator()) {}

    /**
     * @brief Конструктор из компаратора и аллокатора
     *
     * @param comp - компаратор
     * @param alloc - аллокатор
     *
     * @exception Любые исключения от конструктора копирования аллокатора или компаратора
     * @exception std::bad_alloc при невозможности выделения памяти
     */
    explicit Map(const Compare& comp,
                 const Allocator& alloc = Allocator()) :
        imaginary_(create_imaginary()),
        size_(0),
        comp_(comp),
        alloc_(alloc),
        node_alloc_(alloc),
        base_alloc_(alloc) {}

    /**
     * @brief Map - конструктор от аллокатора
     *
     * @param alloc - аллокатор
     */
    explicit Map(const Allocator& alloc) :
        imaginary_(create_imaginary()),
        size_(0),
        comp_(Compare()),
        alloc_(alloc),
        node_alloc_(alloc),
        base_alloc_(alloc) {}

    /**
     * @brief Map - конструктор от диапазона
     *
     * @param first - итератор на начало диапазона
     * @param last - итератор на конец диапазона
     * @param comp - компаратор
     * @param alloc - аллокатор
     */
    template<typename InputIt>
    Map(InputIt first, InputIt last, const Compare& comp = Compare(), const Allocator& alloc = Allocator()) :
        imaginary_(create_imaginary()),
        size_(0),
        comp_(comp),
        alloc_(alloc),
        node_alloc_(alloc),
        base_alloc_(alloc)
    { for(auto it = first; it != last; ++it) emplace(*it); }

    /**
     * @brief Map - конструктор от диапазона с подменой аллокатора
     *
     * @param first - итератор на начало диапазона
     * @param last - итератор на конец диапазона
     * @param alloc - аллокатор
     */
    template<typename InputIt>
    Map(InputIt first, InputIt last, const Allocator& alloc) :
        imaginary_(create_imaginary()),
        size_(0),
        comp_(Compare()),
        alloc_(alloc),
        node_alloc_(alloc),
        base_alloc_(alloc)
    { for(auto it = first; it != last; ++it) emplace(*it); }

    /**
     * @brief Map - конструктор от std::initializer_list<std::pair<const Key, T>>
     *
     * @param init - список инициализации
     * @param comp - компаратор
     * @param alloc - аллокатор
     */
    Map(std::initializer_list<value_type> init,
        const Compare& comp = Compare(),
        const Allocator& alloc = Allocator()) :
        imaginary_(create_imaginary()),
        size_(0),
        comp_(comp),
        alloc_(alloc),
        node_alloc_(alloc),
        base_alloc_(alloc)
    { for(auto v : init) emplace(std::move(v)); }

    /**
     * @brief Map - конструктор от std::initializer_list<std::pair<const Key, T>> с подменой аллокатора
     *
     * @param init - список инициализации
     * @param alloc - аллокатор
     */
    Map(std::initializer_list<value_type> init, const Allocator& alloc) :
        imaginary_(create_imaginary()),
        size_(0),
        comp_(Compare()),
        alloc_(alloc),
        node_alloc_(alloc),
        base_alloc_(alloc)
    { for(auto v : init) emplace(std::move(v)); }

    /**
     * @brief Конструктор копирования
     *
     * @param other - другой mystl::Map
     *
     * @exception Любые исключения от конструктора копирования аллокатора или компаратора
     * @exception std::bad_alloc при невозможности выделения памяти
     * @exception Любые исключения от конструктора копирования Key, T
     */
    Map(const Map& other)
        requires std::copy_constructible<std::pair<const Key, T>> :
        imaginary_(create_imaginary()),
        size_(other.size_),
        comp_(other.comp_),
        alloc_(std::allocator_traits<Allocator>::select_on_container_copy_construction(other.alloc_)),
        node_alloc_(other.alloc_),
        base_alloc_(other.alloc_)
    {
        try {
            imaginary_->left_ = cloner(other.imaginary_->left_, imaginary_);
        } catch(...) {
            if (imaginary_) {
                clear();
                destroy_imaginary(imaginary_);
            }
            throw;
        }
    }

    /**
     * @brief Map - конструктор перемещения
     *
     * @param other - другой mystl::map
     *
     * @exception std::bad_alloc при неудачном выделении памяти
     * @exception Любые исключения, связанные с копированием аллокатора или компаратора
     */
    Map(Map&& other)
        : imaginary_(other.imaginary_),
        size_(other.size_),
        comp_(std::move(other.comp_)),
        alloc_(std::move(other.alloc_)),
        node_alloc_(std::move(other.node_alloc_)),
        base_alloc_(std::move(other.base_alloc_))
    {
        other.comp_ = Compare();
        other.alloc_ = Allocator();
        other.node_alloc_ = node_allocator();
        other.base_alloc_ = base_allocator();
        other.imaginary_ = create_imaginary();
        other.size_ = 0;
    }

    /**
     * @brief operator = - копирующий опревтор присваивания
     *
     * @param other - другой mystl::Map
     *
     * @return Map& - ссылка на себя
     */
    Map& operator = (const Map& other) {
        if (this != &other) {
            Map temp(other);
            *this = std::move(temp);
        }
        return *this;
    }

    /**
     * @brief operator = - перемещающий оператор присваивания
     *
     * @param other - другой mystl::Map
     *
     * @return Map& - ссылка на себя
     */
    Map& operator = (Map&& other) {
        if(this != &other) {
            BaseNode* new_imaginary = other.imaginary_;
            size_t new_size = other.size_;

            std::swap(imaginary_, new_imaginary);
            std::swap(size_, new_size);
            comp_ = std::move(other.comp_);
            alloc_ = std::move(other.alloc_);
            node_alloc_ = std::move(other.node_alloc_);
            base_alloc_ = std::move(other.base_alloc_);

            if (new_imaginary) {
                clear();
                destroy_imaginary(new_imaginary);
            }

            other.comp_ = Compare();
            other.alloc_ = Allocator();
            other.node_alloc_ = node_allocator();
            other.base_alloc_ = base_allocator();
            other.imaginary_ = create_imaginary();
            other.size_ = 0;
        }
        return *this;
    }

    // FINDER BLOCK

private:

    struct FindResult {
        BaseNode* parent;   // куда вставлять
        bool is_left;       // слева или справа
        Node* existing;     // найденный ключ или nullptr
    };

    /**
     * @brief finder - поиск элемента в дереве
     *
     * @param key - ключ
     *
     * @return FindResult
     */
    FindResult finder(const Key& key) const noexcept {
        BaseNode* cur = imaginary_->left_;
        BaseNode* parent = imaginary_;
        bool is_left = true;

        while (cur != nullptr) {
            Node* n = static_cast<Node*>(cur);
            if (comp_(key, n->value_.first)) { // key < n->value_.first
                parent = cur;
                cur = cur->left_;
                is_left = true;
            } else if (comp_(n->value_.first, key)) { // key > n->value_.first
                parent = cur;
                cur = cur->right_;
                is_left = false;
            } else {
                return { parent, is_left, n };
            }
        }
        return { parent, is_left, nullptr };
    }

public:

    /**
     * @brief find - поиск по ключу
     *
     * @param key - ключ
     *
     * @return iterator на std::pair<const Key, T>
     */
    iterator find(const Key& key) noexcept {
        auto [_, __, ptr] = finder(key);
        if(ptr != nullptr) return iterator(ptr);
        else return end();
    }

    /**
     * @brief find - поиск по ключу
     *
     * @param key - ключ
     *
     * @return const_iterator на const std::pair<const Key, T>
     */
    const_iterator find(const Key& key) const noexcept {
        const auto [_, __, ptr] = finder(key);
        if(ptr != nullptr) return const_iterator(ptr);
        else return end();
    }


    // RED-BLACK TREE BLOCK

private:

    /**
     * @brief is_red - Узел красный?
     *
     * @param node
     *
     * @return true, если узел красный
     */
    bool is_red(BaseNode* node) const noexcept {
        // nullptr считается черным узлом (листом)
        return node != nullptr && node->is_red_;
    }

    /**
     * @brief verify_subtree - рекурсивный обход поддерева с проверкой инвариантов КЧ-дерева
     *
     * @param node - корень поддерева
     *
     * @return int - черная высота
     *
     * @throws std::logic_error в случае, если КЧ-дерево невалидно
     */
    int verify_subtree(BaseNode* node) const {
        if (node == nullptr) return 1; // nullptr (лист) имеет черную высоту 1

        if (is_red(node) && (is_red(node->left_) || is_red(node->right_)))
            throw std::logic_error("There are two red nodes in a row;");

        int left_black_height = verify_subtree(node->left_);
        int right_black_height = verify_subtree(node->right_);

        if (left_black_height != right_black_height) throw std::logic_error("Black height mismatch between subtrees;");

        return left_black_height + (is_red(node) ? 0 : 1);
    }

public:

    /**
     * @brief invariants_checker - обход всего дерева с целью проверки инвариантов КЧ-дерева
     *
     * @exception std::logic_error в случае, если КЧ-дерево невалидно
     */
    void invariants_checker() const {
        if(is_red(imaginary_)) throw std::logic_error("Imaginary node must be black, but it is red now;");
        if(is_red(imaginary_->left_)) throw std::logic_error("Root is not black;");

        try {
            verify_subtree(imaginary_->left_);
        } catch (const std::logic_error& e) {
            throw std::logic_error(std::string("RB-tree invariant violation: ") + e.what());
        }
    }

private:

    /**
     * @brief rotate_left - левый поворот вокруг ноды x
     *
     * @param x
     */
    void rotate_left(BaseNode* x) noexcept {
        BaseNode* y = x->right_;  // 1. Фиксируем правого ребенка

        // 2. Перемещаем поддерево B
        x->right_ = y->left_;
        if (y->left_) {
            y->left_->parent_ = x;
        }

        // 3. Устанавливаем родителя Y
        y->parent_ = x->parent_;
        if (x->parent_ == imaginary_) {
            // X был корнем
            imaginary_->left_ = y;
        } else if (x == x->parent_->left_) {
            x->parent_->left_ = y;
        } else {
            x->parent_->right_ = y;
        }

        // 4. Делаем X левым ребенком Y
        y->left_ = x;
        x->parent_ = y;
    }

    /**
     * @brief rotate_right - правый поворот вокруг ноды x
     *
     * @param x
     */
    void rotate_right(BaseNode* x) noexcept {
        BaseNode* y = x->left_;  // 1. Фиксируем левого ребенка

        // 2. Перемещаем поддерево B
        x->left_ = y->right_;
        if (y->right_) {
            y->right_->parent_ = x;
        }

        // 3. Устанавливаем родителя Y
        y->parent_ = x->parent_;
        if (x->parent_ == imaginary_) {
            // X был корнем
            imaginary_->left_ = y;
        } else if (x == x->parent_->left_) {
            x->parent_->left_ = y;
        } else {
            x->parent_->right_ = y;
        }

        // 4. Делаем X правым ребенком Y
        y->right_ = x;
        x->parent_ = y;
    }

    // EMPLACE BLOCK

    /**
     * @brief emplace_balancer - валидирует КЧ-дерево при добавлении нового элемента
     *
     * @param node - указатель на вставленный узел
     */
    void emplace_balancer(BaseNode* node) noexcept {
        while (node->parent_ != imaginary_ && is_red(node->parent_)) {
            BaseNode* parent = node->parent_;
            BaseNode* grandparent = parent->parent_;
            //            /|\
            //             |
            // Забавен тот факт, что условие цикла while НИКОГДА не
            // допустит ситуации (grandparent == imaginary_) => true.
            // Таким образом, дальнейшие повороты БЕЗОПАСНЫ.

            if (parent == grandparent->left_) {
                BaseNode* uncle = grandparent->right_;
                // Case 1:
                //
                //                                  ...
                //                                /    \
                //                [black]grandparent    ...
                //                     /       \
                //            [red]parent     [?]uncle
                //         (!) ↕ /       \      /    \
                // inserted-> [red]node  a...  b...  c...
                //          /      \
                //     [black]  [black]
                //     nullptr  nullptr
                //

                if (is_red(uncle)) {
                    // Case 1.1: Дядя красный
                    //
                    //                             ...
                    //                           /    \
                    //           [black]grandparent    ...
                    //                 /       \
                    //         [red]parent   [red]uncle
                    //           /      \     /    \
                    //    [red]node    a...  b...  c...
                    //      /      \
                    // [black]  [black]
                    // nullptr  nullptr
                    //                              |
                    parent->is_red_ = false;     // |
                    uncle->is_red_ = false;      // |
                    grandparent->is_red_ = true; // |
                    node = grandparent;          // |
                    //                             \|/
                    //
                    //                             ...
                    //                           /    \
                    //            [red]grandparent    ...
                    //                 /       \
                    //       [black]parent  [black]uncle
                    //           /      \     /    \
                    //    [red]node   a...  b...   c...
                    //      /      \
                    // [black]  [black]
                    // nullptr  nullptr
                    //
                    continue;
                }

                // Case 1.2: Дядя черный
                if (node == parent->right_) {
                    // Case 1.2.2: Зигзаг (левый-правый случай)
                    //
                    //                             ...
                    //                           /    \
                    //           [black]grandparent    ...
                    //                 /       \
                    //         [red]parent   [black]uncle
                    //          /       \         /     \
                    //       a...   [red]node    b...    c...
                    //             /      \
                    //           [black]  [black]   |
                    //           nullptr  nullptr   |
                    rotate_left(parent);//         \|/
                    //
                    //                             ...
                    //                           /    \
                    //           [black]grandparent    ...
                    //                 /       \
                    //         [red]node   [black]uncle
                    //          /       \         /     \
                    //     [red]parent  a...    b...    c...
                    //     /      \
                    //  [black]  [black]
                    //  nullptr  nullptr
                    //
                    node = parent;
                    parent = node->parent_;
                }

                // Case 1.2.1:
                //
                //                             ...
                //                           /    \
                //           [black]grandparent    ...
                //                 /       \
                //         [red]parent   [black]uncle
                //           /      \     /    \
                //    [red]node    a...  b...    c...
                //      /      \
                // [black]  [black]
                // nullptr  nullptr             |
                //                              |
                rotate_right(grandparent);  //  |
                parent->is_red_ = false;    //  |
                grandparent->is_red_ = true;// \|/
                //
                //                             ...
                //                           /    \
                //              [black]parent     ...
                //                 /       \
                //         [red]node   [red]grandparent
                //           /      \          /      \
                //    [black]     [black]     a...  [black]uncle
                //    nullptr     nullptr                 /  \
                //                                     b...   c...
                //
            } else {
                // Симметричный случай: родитель - правый ребенок
                BaseNode* uncle = grandparent->left_;

                // Case 2.1: Дядя красный
                if (is_red(uncle)) {
                    parent->is_red_ = false;
                    uncle->is_red_ = false;
                    grandparent->is_red_ = true;
                    node = grandparent;
                    continue;
                }
                // Case 2.2.2: Узел - левый ребенок
                if (node == parent->left_) {
                    rotate_right(parent);
                    node = parent;
                    parent = node->parent_;
                }

                // Case 2.2.1: Узел - правый ребенок
                rotate_left(grandparent);
                parent->is_red_ = false;
                grandparent->is_red_ = true;
            }
        }

        // Красим корень в черный, соблюдая инвариант
        if (imaginary_->left_) imaginary_->left_->is_red_ = false;
    }

public:

    /**
     * @brief emplace - сборка элемента из переданных параметров
     *
     * @param args - кортеж параметров
     *
     * @return std::pair<iterator, bool> - итератор на собранный элемент и bool (вставлен ли элемент)
     *
     * @exception Любые исключения от конструктора из Args...
     */
    template< class... Args >
    requires std::constructible_from<std::pair<const Key, T>, Args...>
    std::pair<iterator, bool> emplace(Args&&... args) {

        auto new_node = create_node(std::forward<Args>(args)...);  // Прямая передача

        auto res = finder(new_node->value_.first);

        if (res.existing != nullptr) {
            std::allocator_traits<node_allocator>::destroy(node_alloc_, new_node);
            std::allocator_traits<node_allocator>::deallocate(node_alloc_, new_node, 1);
            return { iterator(res.existing), false };
        }

        // Устанавливаем связь с родителем
        if (res.is_left) res.parent->left_  = new_node;
        else             res.parent->right_ = new_node;
        new_node->parent_ = res.parent;

        new_node->is_red_ = true;

        emplace_balancer(new_node);

        ++size_;

        return { iterator(new_node), true };
    }

    /**
     * @brief insert - вставка пары элементов
     *
     * @param kv - пара const Key, T
     *
     * @return std::pair<iterator, bool> - итератор на собранный элемент и bool (вставлен ли элемент)
     *
     * @exception Любые исключения от конструктора копирования Key, T
     */
    std::pair<iterator, bool> insert(const std::pair<const Key, T>& kv) { return emplace(kv); }

    // ERASE BLOCK

private:

    /**
     * @brief eraser - механизм удаления узла по указаьтелю
     *
     * Удаляет узел из бинарного дерева поиска по классическому алгоритму и
     * при необходимости вызывает балансировщик для поддержания инвариантов КЧ-дерева
     *
     * @param node - указатель на удалемый узел
     */
    void eraser(BaseNode* node) noexcept {

        BaseNode* node_for_balancing = nullptr;  // Узел, с которого начнется балансировка
        BaseNode* nfb_ancestor = nullptr;
        bool nfb_is_left = false;

        bool original_color = is_red(node);  // Сохраняем оригинальный цвет

        //
        // Случай 0: нет детей
        //
        if(node->right_ == nullptr && node->left_ == nullptr) {

            node_for_balancing = nullptr;
            nfb_ancestor = node->parent_;
            nfb_is_left = (node == node->parent_->left_);

            if (node == node->parent_->left_) node->parent_->left_  = nullptr;
            else                              node->parent_->right_ = nullptr;

        }
        //
        // Случай 1: единственный ребенок
        //
        // 1.1: Правый потомок
        else if (node->right_ != nullptr && node->left_ == nullptr) {

            node_for_balancing = node->right_;
            nfb_ancestor = node->parent_;
            nfb_is_left = (node == node->parent_->left_);

            // 1.1.1 Узел - левый потомок
            if (node == node->parent_->left_) node->parent_->left_  = node_for_balancing;
            //1.1.2 Узел - правый потомок
            else                              node->parent_->right_ = node_for_balancing;

            // Восстанавливаем связь с родителем
            node_for_balancing->parent_ = node->parent_;

        }
        // 1.2: Левый ребеной (симметрично 1.1)
        else if (node->right_ == nullptr && node->left_ != nullptr) {
            node_for_balancing = node->left_;
            nfb_ancestor = node->parent_;
            nfb_is_left = (node == node->parent_->left_);

            if (node == node->parent_->left_) node->parent_->left_  = node_for_balancing;
            else                              node->parent_->right_ = node_for_balancing;

            node_for_balancing->parent_ = node->parent_;
        }
        //
        // Cлучай 2: два ребенка
        //
        else {
            // Ищеем замену(самый левый узел в правом поддереве)
            BaseNode* replacement = node->right_; // не nullptr гарантированно

            // Идем в самое левое поддерево
            while (replacement->left_ != nullptr) replacement = replacement->left_;

            original_color = is_red(replacement);

            // node_for_balancing - тот узел, который ЗАНЯЛ МЕСТО УДАЛЕННОГО
            node_for_balancing = replacement->right_;  // Может быть nullptr

            // 2.1: преемник - не непосрественный потомок node
            //                ...
            //                /
            //              node
            //             /    \
            //          a...    b... <- (b...) не пусто
            //                 /   \
            //        replacement  c...
            //         /    \
            //   nullptr    d...
            //
            if (replacement != node->right_) {

                nfb_ancestor = replacement->parent_;
                nfb_is_left = true;

                // Заменяем преемника его правым ребенком:
                //                ...
                //                /
                //              node
                //             /    \
                //          a...    b... <- (b...) не пусто
                //                 /   \
                //               d...   c...
                //             /    \
                //           ...    ...
                //

                replacement->parent_->left_ = replacement->right_;

                // Восстановление связи с родителем
                if (replacement->right_ != nullptr) replacement->right_->parent_ = replacement->parent_;

                // Присоединяем правое поддерево удаляемого узла к преемнику
                replacement->right_ = node->right_;
                node->right_->parent_ = replacement;

            }
            // 2.2: Преемник является непосредственным правым ребенком удаляемого узла
            else {

                // В этом случае node_for_balancing уже равен successor->right_
                nfb_ancestor = replacement;
                nfb_is_left = false;

            }

            // ЗАМЕНА: Заменяем удаляемый узел на преемника у родителя удаляемого узла
            //                ...
            //                /
            //           replacement
            //             /    \
            //          a...    b... <- (b...) не пусто
            //                 /   \
            //               d...   c...
            //             /    \
            //           ...    ...
            //

            // 1. Определяем, был ли удаляемый узел левым или правым ребенком своего родителя
            if (node == node->parent_->left_) node->parent_->left_  = replacement;
            else                              node->parent_->right_ = replacement;

            // 2. Устанавливаем родителя преемника
            replacement->parent_ = node->parent_;

            // Присоединяем левое поддерево удаляемого узла к преемнику
            replacement->left_ = node->left_;
            if(node->left_ != nullptr) node->left_->parent_ = replacement;

            // Копируем цвет удаляемого узла в преемника
            replacement->is_red_ = is_red(node);
        }

        // Освобождаем память удаленного узла
        Node* real = static_cast<Node*>(node);
        std::allocator_traits<node_allocator>::destroy(node_alloc_, real);
        std::allocator_traits<node_allocator>::deallocate(node_alloc_, real, 1);
        --size_;

        // Позвать балансировщика
        if (!original_color) erase_balancer(node_for_balancing, nfb_ancestor, nfb_is_left);
    }

    /**
     * @brief Балансировка дерева после удаления черного узла
     *
     * @param node Узел, с которого начинается балансировка
     */
    void erase_balancer(BaseNode* x, BaseNode* x_parent, bool x_is_left) noexcept {
        // Начальные локальные переменные: будем поддерживать parent отдельно,
        // потому что x может быть nullptr.
        BaseNode* node   = x;
        BaseNode* parent = x ? x->parent_ : x_parent;
        bool is_left = x_is_left;

        // Цикл: пока x не корень и x чёрный
        while ((parent != imaginary_) && !is_red(node)) {

            // Если node — левый ребёнок parent
            //              ...
            //            /    \
            //   [Black]node   ...
            //   /   \
            // ...   ...
            //
            if (is_left) {
                BaseNode* brother = parent->right_;

                // Case 1: брат красный — поворот вокруг parent, затем обновляем указатели
                //
                //               ...
                //               /
                //           [Black]parent
                //           /       \
                //      [Black]node  [Red]brother
                //      /   \           /       \
                //    a...  b...      [Black]    e...
                //                    important
                //                      /   \
                //                    c...  d...
                //
                if (is_red(brother)) {
                    // Перекраска
                    brother->is_red_ = false;
                    parent->is_red_ = true;

                    // Поворот влево вокруг parent
                    rotate_left(parent);

                    // После поворота связи изменились: обновим brother
                    // node не меняется (он всё ещё тот же самый узел)
                    brother = parent->right_;
                }
                //                   ...
                //                   /
                //           [Black]old_brother
                //             /          \
                //          [Red]parent   e...
                //           /        \
                //      [Black]node   [Black]important(теперь это brother)
                //      /   \              /    \
                //    a...  b...          c...  d...
                //

                // Дальше предполагается, что brother — чёрный (или nullptr)
                BaseNode* b_left  = brother ? brother->left_  : nullptr;
                BaseNode* b_right = brother ? brother->right_ : nullptr;

                // Case 2: оба ребёнка брата чёрные
                //                   ...
                //                 /     \
                //          [?]parent    ...
                //           /       \
                //      [Black]node   [black]brother
                //      /   \          /     \
                //    a...  b...   [black]  [black]
                //                 b_left   b_right
                //                  /  \     /  \
                //                ... ...  ... ...
                //
                if (!is_red(b_left) && !is_red(b_right)) {
                    if (brother != nullptr) brother->is_red_ = true; // если brother == nullptr — ничего не делаем
                    // переносим проблему выше: node = parent
                    node = parent;
                    parent = node->parent_;
                    is_left = (node == parent->left_);
                    continue; // продолжить цикл с новым node/parent
                }
                //  Если node красный, то после
                //  выхода из цикла node станет черным
                //  и конфликт c brother разрешится
                //            |
                //            |      ...
                //            |      /
                //            |  [?]parent
                //           \|/  /       \
                //           [?]node     d...
                //           /       \
                //   [Black]old_node  [Red]brother
                //      /   \          /     \
                //    a...  b...   [black]  [black]
                //                 b_left   b_right
                //                  /  \     /  \
                //                ... ...  ... ...
                //

                else
                {
                    // Case 3: правый ребёнок брата чёрный
                    //                     ...
                    //                   /    \
                    //          [?]parent     ...
                    //           /       \
                    //      [Black]node   [Black]brother
                    //      /   \          /           \
                    //    ...  ...     [Red]           [Black]
                    //                 b_left          b_right
                    //                  /   \            /   \
                    //           [Black]    [Black]     a... b...
                    //           important1 important2
                    //             /  \       /  \
                    //           ...  ...   ...  ...
                    //
                    if (!is_red(b_right)) {
                        // Здесь гарантированно b_left != nullptr && is_red(b_left) == true
                        // Сделаем перекраску и малый поворот вокруг brother
                        b_left->is_red_ = false;
                        brother->is_red_ = true;
                        rotate_right(brother);

                        // Обновим локальные указатели после rotate_right
                        parent  = node ? node->parent_ : x_parent;
                        brother = parent->right_;
                        b_right = brother ? brother->right_ : nullptr;
                        //b_left далее не задействован, персчет необязателен
                    }
                    //                   ...
                    //                 /     \
                    //          [?]parent    ...
                    //           /      \
                    //   [Black]node   [Black]new_brother=b_left
                    //      /   \          /        \
                    //    ...  ...     [Black]      [Red]old_brother
                    //                 important1        /        \
                    //                  /   \       [Black]      [Black]
                    //                 ...  ...     important2   b_right
                    //                                /  \        /   \
                    //                              ...  ...     a... b...
                    //

                    // Case 4: Правый ребенок брата красный
                    //               ...
                    //               /
                    //            [?]parent
                    //           /        \
                    //      [Black]node  [Black]brother
                    //      /   \          /     \
                    //    a...  b...   c...   [Red]b_right
                    //                          /   \
                    //                      d...    e...
                    //

                    // Перекраска и левый поворот вокруг parent
                    brother->is_red_ = is_red(parent);

                    parent->is_red_  = false;
                    b_right->is_red_ = false;

                    rotate_left(parent);

                    // После решающего поворота мы можем закончить —
                    // установить node = root и выйти
                    node = imaginary_->left_;

                    //              ...
                    //              /
                    //           [?]brother  <--- Конфликта выше(красный<->красный) не будет,
                    //            /       \       так как раньше(до поворота) конфликта не было
                    //           /         \
                    //      [Black]parent  [Black]b_right
                    //         /       \        /    \
                    //     [Black]node  c...  d...   e...
                    //      /   \
                    //    a...  b...
                    //
                }
            } else {
                // node - правый ребенок parent
                BaseNode* brother = parent->left_;

                if (is_red(brother)) {
                    brother->is_red_ = false;
                    parent->is_red_ = true;
                    rotate_right(parent);

                    brother = parent->left_;
                }

                BaseNode* b_left = brother ? brother->left_ : nullptr;
                BaseNode* b_right = brother ? brother->right_ : nullptr;

                if (!is_red(b_left) && !is_red(b_right)) {
                    if (brother != nullptr) brother->is_red_ = true;
                    node = parent;
                    parent = node->parent_;
                    is_left = (node == parent->left_);
                    continue;
                }

                if (!is_red(b_left)) {
                    if (b_right != nullptr) b_right->is_red_ = false;
                    brother->is_red_ = true;
                    rotate_left(brother);

                    brother = parent->left_;
                    b_left = brother ? brother->left_ : nullptr;
                }

                brother->is_red_ = is_red(parent);
                parent->is_red_ = false;
                if (b_left != nullptr) b_left->is_red_ = false;

                rotate_right(parent);
                node = imaginary_->left_;
                break;
            }
        }

        if (node != nullptr) node->is_red_ = false;  // Восстанавливает все свойства
    }

public:

    /**
     * @brief erase - удаляет элемент, на который указывает итератор
     *
     * @param position - итератор на удаляемый элемент
     *
     * @return iterator на следующий элемент
     */
    iterator erase(iterator position) noexcept {
        if (position == end()) return end();

        auto node_to_delete = position.base();
        iterator next = position;
        ++next;

        eraser(node_to_delete);
        return next;
    }

    /**
     * @brief erase -  удаляет элемент по ключу
     *
     * @param key - ключ
     *
     * @return std::size_t - число удаленных элементов
     */
    std::size_t erase(const Key& key) noexcept {
        auto it = find(key);
        if(it != end()) {
            erase(it);
            return 1;
        } else {
            return 0;
        }
    }

    // ACCESS BLOCK

    /**
     * @brief at - доступ по ключу(бросает исключение)
     *
     * @param key - ключ
     *
     * @exception std::out_of_range в случае, если Map не содержит key
     *
     * @return T&
     */
    T& at(const Key& key) {
        auto res = finder(key);
        if (res.existing != nullptr) return res.existing->value_.second;
        else throw std::out_of_range("Map doesent contains such element");
    }

    /**
     * @brief at - доступ по ключу(бросает исключение)
     *
     * @param key - ключ
     *
     * @exception std::out_of_range в случае, если Map не содержит key
     *
     * @return T&
     */
    const T& at(const Key& key) const {
        auto res = finder(key);
        if (res.existing != nullptr) return res.existing->value_.second;
        else throw std::out_of_range("Map doesent contains such element");
    }

    /**
     * @brief operator [] - вставляет пару (key, T()), если key, иначе - возвращает знацение по ключу
     *
     * @param key - ключ
     *
     * @return T&
     */
    T& operator[](const Key& key) noexcept {
        auto res = finder(key);
        if (res.existing != nullptr) return res.existing->value_.second;
        else {
            auto [it, _] = emplace(key, T());
            return it->second;
        }
    }

    //ETC BLOCK

    /**
     * @brief contains - проверяет есть ли ключ в дереве
     *
     * @param key - ключ
     *
     * @return true, если ключ есть, иначе false
     */
    bool contains(const Key& key) const noexcept {
        auto it = find(key);
        if(it != end()) return true;
        else return false;
    }

    /**
     * @brief size - количество пар в дереве
     *
     * @return std::size_t
     */
    std::size_t size() const noexcept { return size_; }

    /**
     * @brief empty - Пуст ли контейнер?
     *
     * @return bool true, если пусто
     */
    bool empty() const noexcept { return size_ == 0; }

    /**
     * @brief clear - очистка контейнера
     */
    void clear() noexcept { cleaner(imaginary_->left_); size_ = 0; }
};

}
#endif // MAP_HPP
