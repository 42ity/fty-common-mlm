#pragma once
#include "fty_common_mlm_tntmlm.h"
#include <mutex>

namespace details {

template <typename ObjectType>
class Pool
{
public:
    class Ptr
    {
        friend class Pool;

        std::unique_ptr<ObjectType> m_object = nullptr;
        Pool*                       m_pool   = nullptr;

        void doUnlink()
        {
            if (m_pool) {
                m_pool->put(*this);
            }
        }

    public:
        Ptr(std::unique_ptr<ObjectType>&& ptr, Pool* pool)
            : m_object(std::move(ptr))
            , m_pool(pool)
        {
        }

        Ptr(const Ptr& ptr) = delete;

        Ptr(Ptr&& ptr)
            : m_object(std::move(ptr.m_object))
            , m_pool(std::move(ptr.m_pool))
        {
        }

        ~Ptr()
        {
            doUnlink();
        }

        Ptr& operator=(const Ptr& ptr) = delete;
        Ptr& operator=(Ptr&& ptr)
        {
            if (m_object != ptr.m_object) {
                doUnlink();
                m_object = std::move(ptr.m_object);
                m_pool   = std::move(ptr.m_pool);
            }
            return *this;
        }

        ObjectType* operator->() const
        {
            return m_object.get();
        }

        ObjectType& operator*() const
        {
            return *m_object.get();
        }

        bool operator==(const ObjectType* p) const
        {
            return m_object == p;
        }

        bool operator!=(const ObjectType* p) const
        {
            return m_object.get() != p;
        }

        bool operator<(const ObjectType* p) const
        {
            return m_object < p;
        }

        bool operator!() const
        {
            return m_object == 0;
        }

        operator bool() const
        {
            return m_object != 0;
        }

        operator ObjectType*()
        {
            return m_object.get();
        }

        operator const ObjectType*() const
        {
            return m_object.get();
        }

        void setPool(Pool* p)
        {
            m_pool = p;
        }

        void release()
        {
            m_pool = nullptr;
        }

        ObjectType* getPointer()
        {
            return m_object.get();
        }

        const ObjectType* getPointer() const
        {
            return m_object.get();
        }
    };

private:
    using Container = std::vector<std::unique_ptr<ObjectType>>;

    Container          m_freePool;
    unsigned           m_maxSpare;
    mutable std::mutex m_mutex;

    bool put(Ptr& po) // returns true, if object was put into the freePool vector
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_maxSpare == 0 || m_freePool.size() < m_maxSpare) {
            po.setPool(nullptr);
            m_freePool.emplace_back(std::move(po.m_object));
            return true;
        }

        return false;
    }

public:
    explicit Pool(unsigned maxSpare = 0)
        : m_maxSpare(maxSpare)
    {
    }

    ~Pool()
    {
        m_freePool.clear();
    }

    Ptr get()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_freePool.empty()) {
            return Ptr(std::make_unique<ObjectType>(), this);
        } else {
            auto getter = [this]() {
                auto it = std::move(m_freePool.back());
                m_freePool.pop_back();
                return it;
            };
            return Ptr(std::move(getter()), this);
        }
    }

    void drop(unsigned keep = 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_freePool.size() > keep) {
            m_freePool.resize(keep);
        }
    }

    unsigned getMaximumSize() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_maxSpare;
    }

    unsigned size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_freePool.size();
    }

    unsigned getCurrentSize() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_freePool.size();
    }

    void setMaximumSize(unsigned s)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_maxSpare = s;
        if (m_freePool.size() > s)
            m_freePool.resize(s);
    }
};

} // namespace details

using MlmClientPool = details::Pool<MlmClient>;
extern MlmClientPool mlm_pool;
