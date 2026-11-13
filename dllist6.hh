#ifndef DLIST6_HPP
#define DLIST6_HPP

#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>

/* --- Стратегии размещения узлов ---------------------------------------- */

template <class T>
class HeapNodeSource {
public:
    void *allocate(std::size_t bytes) { return ::operator new(bytes); }
    void deallocate(void *p) noexcept { ::operator delete(p); }
};

template <class T, std::size_t BlockNodes = 1024>
class PoolNodeSource {
public:
    PoolNodeSource() : blocks_(nullptr), free_(nullptr), node_size_(0) {}
    ~PoolNodeSource() { free_blocks(); }
    PoolNodeSource(const PoolNodeSource &) = delete;
    PoolNodeSource &operator=(const PoolNodeSource &) = delete;

    void *allocate(std::size_t bytes)
    {
        if (free_ == nullptr)
            new_block(bytes);
        Slot *s = free_;
        free_ = s->next;
        return s;
    }

    void deallocate(void *p) noexcept
    {
        Slot *s = static_cast<Slot *>(p);
        s->next = free_;
        free_ = s;
    }

private:
    struct Slot  { Slot *next; };
    struct Block { Block *next; char *mem; };

    void new_block(std::size_t bytes)
    {
        node_size_ = bytes;
        Block *b = new Block;
        b->mem = static_cast<char *>(::operator new(bytes * BlockNodes));
        b->next = blocks_;
        blocks_ = b;
        for (std::size_t i = 0; i < BlockNodes; ++i) {
            Slot *s = reinterpret_cast<Slot *>(b->mem + i * bytes);
            s->next = free_;
            free_ = s;
        }
    }

    void free_blocks() noexcept
    {
        while (blocks_ != nullptr) {
            Block *next = blocks_->next;
            ::operator delete(blocks_->mem);
            delete blocks_;
            blocks_ = next;
        }
        free_ = nullptr;
    }

    Block *blocks_;
    Slot *free_;
    std::size_t node_size_;
};

/* --- Список ------------------------------------------------------------ */

/* Тип без конструктора по умолчанию — для проверки шаблона.
   Определён в заголовке, чтобы им могли пользоваться и реализация, и демо. */
struct NoDefault {
    explicit NoDefault(int v) : x(v) {}
    NoDefault() = delete;
    bool operator<(const NoDefault &o) const { return x < o.x; }
    int x;
};

template <class T, class NodeSource = HeapNodeSource<T>>
class DList {
private:
    /* Узлы объявлены ДО итераторов: вложенный класс видит только
       уже объявленные имена окружающего класса. Страж не хранит T,
       поэтому T не обязан иметь конструктор по умолчанию. */
    struct NodeBase {
        NodeBase *next;
        NodeBase *prev;
    };
    struct Node : NodeBase {
        explicit Node(const T &v) : value(v) {}
        T value;
    };

public:
    using Value = T;   /* нужен декораторам */

    explicit DList(std::function<bool(const T &, const T &)> cmp = nullptr)
        : comparator_(cmp), size_(0)
    {
        sentinel_.next = &sentinel_;
        sentinel_.prev = &sentinel_;
    }

    ~DList() { clear(); }

    /* Копирование отключено: источник узлов владеет памятью */
    DList(const DList &) = delete;
    DList &operator=(const DList &) = delete;

    void push_front(const T &v) { insert_after(&sentinel_, v); }
    void push_back(const T &v)  { insert_after(sentinel_.prev, v); }

    T pop_front()
    {
        if (size_ == 0)
            throw std::logic_error("DList::pop_front: пусто");
        Node *n = static_cast<Node *>(sentinel_.next);
        T value = n->value;
        unlink(n);
        destroy_node(n);
        --size_;
        return value;
    }

    T pop_back()
    {
        if (size_ == 0)
            throw std::logic_error("DList::pop_back: пусто");
        Node *n = static_cast<Node *>(sentinel_.prev);
        T value = n->value;
        unlink(n);
        destroy_node(n);
        --size_;
        return value;
    }

    void clear()
    {
        while (sentinel_.next != &sentinel_) {
            Node *n = static_cast<Node *>(sentinel_.next);
            sentinel_.next = n->next;
            destroy_node(n);
        }
        sentinel_.prev = &sentinel_;
        size_ = 0;
    }

    void insert_sorted(const T &v)
    {
        NodeBase *pos = &sentinel_;
        while (pos->next != &sentinel_ &&
               less(static_cast<Node *>(pos->next)->value, v))
            pos = pos->next;
        insert_after(pos, v);
    }

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    /* --- Двунаправленный итератор --- */

    class const_iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T *;
        using reference = const T &;

        const_iterator() : node_(nullptr) {}
        reference operator*() const { return static_cast<const Node *>(node_)->value; }
        pointer operator->() const { return &static_cast<const Node *>(node_)->value; }
        const_iterator &operator++() { node_ = node_->next; return *this; }
        const_iterator &operator--() { node_ = node_->prev; return *this; }
        const_iterator operator++(int) { const_iterator t = *this; ++*this; return t; }
        const_iterator operator--(int) { const_iterator t = *this; --*this; return t; }
        bool operator==(const const_iterator &o) const { return node_ == o.node_; }
        bool operator!=(const const_iterator &o) const { return node_ != o.node_; }

    protected:
        friend class DList;
        explicit const_iterator(const void *base)
            : node_(static_cast<const NodeBase *>(base)) {}
        const NodeBase *node_;
    };

    class iterator : public const_iterator {
    public:
        using reference = T &;
        using pointer = T *;

        reference operator*() const
        { return const_cast<Node *>(static_cast<const Node *>(this->node_))->value; }
        pointer operator->() const
        { return &const_cast<Node *>(static_cast<const Node *>(this->node_))->value; }
        iterator &operator++() { ++static_cast<const_iterator &>(*this); return *this; }
        iterator &operator--() { --static_cast<const_iterator &>(*this); return *this; }
        iterator operator++(int) { iterator t = *this; ++*this; return t; }
        iterator operator--(int) { iterator t = *this; --*this; return t; }

    private:
        friend class DList;
        explicit iterator(void *base)
            : const_iterator(static_cast<const void *>(base)) {}
    };

    iterator begin() { return iterator(sentinel_.next); }
    iterator end()   { return iterator(&sentinel_); }
    const_iterator begin() const { return const_iterator(sentinel_.next); }
    const_iterator end() const   { return const_iterator(&sentinel_); }

private:
    NodeBase sentinel_;
    NodeSource source_;
    std::function<bool(const T &, const T &)> comparator_;
    std::size_t size_;

    Node *make_node(const T &v)
    {
        void *mem = source_.allocate(sizeof(Node));
        return new (mem) Node(v);
    }

    void destroy_node(Node *n)
    {
        n->~Node();                        /* разрушаем элемент */
        source_.deallocate(n);
    }

    bool less(const T &a, const T &b) const
    {
        return comparator_ ? comparator_(a, b) : a < b;
    }

    void insert_after(NodeBase *pos, const T &v)
    {
        Node *n = make_node(v);
        n->next = pos->next;
        n->prev = pos;
        pos->next->prev = n;
        pos->next = n;
        ++size_;
    }

    static void unlink(NodeBase *n)
    {
        n->prev->next = n->next;
        n->next->prev = n->prev;
    }
};

/* --- Декораторы --------------------------------------------------------- */

template <class Inner>
class StatsDecorator {
public:
    using Value = typename Inner::Value;
    explicit StatsDecorator(Inner &inner)
        : inner_(inner), pushes_(0), pops_(0), reads_(0) {}

    void push_front(const typename Inner::Value &v) { ++pushes_; inner_.push_front(v); }
    void push_back(const typename Inner::Value &v)  { ++pushes_; inner_.push_back(v); }
    typename Inner::Value pop_front() { ++pops_; return inner_.pop_front(); }
    typename Inner::Value pop_back()  { ++pops_; return inner_.pop_back(); }

    std::size_t size() const { ++reads_; return inner_.size(); }
    bool empty() const { return inner_.empty(); }

    unsigned pushes() const { return pushes_; }
    unsigned pops() const   { return pops_; }
    unsigned reads() const  { return reads_; }

private:
    Inner &inner_;
    unsigned pushes_;
    unsigned pops_;
    mutable unsigned reads_;
};

template <class Inner>
class ReadOnlyDecorator {
public:
    explicit ReadOnlyDecorator(Inner &inner) : inner_(inner) {}
    using Value = typename Inner::Value;
    void push_front(const typename Inner::Value &) { throw std::logic_error("только чтение"); }
    void push_back(const typename Inner::Value &)  { throw std::logic_error("только чтение"); }
    typename Inner::Value pop_front() { throw std::logic_error("только чтение"); }
    typename Inner::Value pop_back()  { throw std::logic_error("только чтение"); }

    std::size_t size() const { return inner_.size(); }
    bool empty() const { return inner_.empty(); }

private:
    Inner &inner_;
};

#endif
