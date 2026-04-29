#include <typeinfo>

namespace __cxxabiv1 {
    class __class_type_info : public std::type_info {
    public:
        virtual ~__class_type_info();
    };
    
    class __si_class_type_info : public __class_type_info {
    public:
        virtual ~__si_class_type_info();
    };
    
    __class_type_info::~__class_type_info() {}
    __si_class_type_info::~__si_class_type_info() {}
}

namespace std {
    type_info::~type_info() {}
    
    bool type_info::operator==(const type_info& rhs) const {
        return this == &rhs || __builtin_strcmp(name(), rhs.name()) == 0;
    }
    
    bool type_info::operator!=(const type_info& rhs) const {
        return !(*this == rhs);
    }
    
    bool type_info::before(const type_info& rhs) const {
        return __builtin_strcmp(name(), rhs.name()) < 0;
    }
    
    const char* type_info::name() const {
        return __name;
    }
    
    size_t type_info::hash_code() const {
        const char* s = name();
        size_t hash = 0;
        while (*s) {
            hash = hash * 31 + *s++;
        }
        return hash;
    }
}
