#pragma once
#include "fty_common_mlm_tntmlm.h"
#include <cxxtools/pool.h>
#include <mutex>

namespace details {

template <typename ObjectType>
class Pool
{
public:
    class Ptr
    {
        ObjectType* m_object = nullptr;
        Pool*       m_pool   = nullptr;

        void doUnlink()
        {
            if (m_pool == nullptr || !m_pool->put(*this)) {
                delete m_object;
            }
        }

    public:
        Ptr() = default;

        Ptr(ObjectType* ptr)
            : m_object(ptr)
        {
        }

        Ptr(const Ptr& ptr)
            : m_object(ptr.m_object)
            , m_pool(ptr.m_pool)
        {
        }

        ~Ptr()
        {
            doUnlink();
        }

        Ptr& operator=(const Ptr& ptr)
        {
            if (m_object != ptr.m_object) {
                doUnlink();
                m_object = ptr.m_object;
                m_pool   = ptr.m_pool;
            }
            return *this;
        }

        ObjectType* operator->() const
        {
            return m_object;
        }

        ObjectType& operator*() const
        {
            return *m_object;
        }

        bool operator==(const ObjectType* p) const
        {
            return m_object == p;
        }

        bool operator!=(const ObjectType* p) const
        {
            return m_object != p;
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

        ObjectType* getPointer()
        {
            return m_object;
        }

        const ObjectType* getPointer() const
        {
            return m_object;
        }

        operator ObjectType*()
        {
            return m_object;
        }

        operator const ObjectType*() const
        {
            return m_object;
        }

        void setPool(Pool* p)
        {
            m_pool = p;
        }

        void release()
        {
            m_pool = nullptr;
        }
    };

private:
    using Container = std::vector<Ptr>;

    Container          m_freePool;
    unsigned           m_maxSpare;
    mutable std::mutex m_mutex;

    bool put(Ptr& po) // returns true, if object was put into the freePool vector
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_maxSpare == 0 || m_freePool.size() < m_maxSpare) {
            po.setPool(nullptr);
            m_freePool.push_back(po);
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

        Ptr po;
        if (m_freePool.empty()) {
            po = Ptr();
        } else {
            po = m_freePool.back();
            m_freePool.pop_back();
        }

        po.setPool(this);
        return po;
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
