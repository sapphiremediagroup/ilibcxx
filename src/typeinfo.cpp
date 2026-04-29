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

__class_type_info::~__class_type_info() = default;
__si_class_type_info::~__si_class_type_info() = default;
}

namespace std {
type_info::~type_info() = default;

bool type_info::__is_pointer_p() const {
    return false;
}

bool type_info::__is_function_p() const {
    return false;
}

bool type_info::__do_catch(const type_info* thr_type, void**, unsigned) const {
    return *this == *thr_type;
}

bool type_info::__do_upcast(const __cxxabiv1::__class_type_info*, void**) const {
    return false;
}

#if !__GXX_TYPEINFO_EQUALITY_INLINE
bool type_info::__equal(const type_info& rhs) const noexcept {
    return name()[0] != '*' && __builtin_strcmp(name(), rhs.name()) == 0;
}
#endif
}
