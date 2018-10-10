#ifndef HOMMEXX_STD_META_HPP
#define HOMMEXX_STD_META_HPP

/*
 *  Emulating c++1z features
 *
 *  This file contains some features that belongs to the std namespace,
 *  and that are likely (if not already scheduled) to be in future standards.
 *  However, we have limited access to c++1z standard on target platforms,
 *  so we can only rely on c++11 features. Everything else that we may need,
 *  we try to emulate here.
 */

#include <memory>

#include "ErrorDefs.hpp"

namespace Homme {

// ================ std::any ================= //

namespace Impl {

class holder_base {
public:
  virtual ~holder_base() = default;

  virtual const std::type_info& type () const = 0;
};

template <typename HeldType>
class holder : public holder_base {
public:
  holder (const HeldType& value) : m_value(value) {}

  template<typename... Args>
  holder (const Args&... args) : m_value(args...) {}

  virtual const std::type_info& type () const { return typeid(HeldType); }

  HeldType& value () { return m_value; }
private:
  HeldType m_value;
};

} // namespace Impl

class any {
public:

  template<typename ConcreteType, typename... Args>
  any (const Args&... args) {
    m_content = std::make_shared<Impl::holder<ConcreteType>>(args...);
  }

  template<typename ConcreteType>
  any (const ConcreteType& src) {
    m_content = std::make_shared<Impl::holder<ConcreteType>>(src);
  }

  std::shared_ptr<Impl::holder_base> content () const { return m_content; }
private:

  std::shared_ptr<Impl::holder_base> m_content;
};

template<typename ConcreteType>
ConcreteType& any_cast (any& src) {
  const auto& src_type = src.content()->type();
  const auto& req_type = typeid(ConcreteType);

  Errors::runtime_check(src_type==req_type, std::string("Error! Invalid cast requested, from '") + src_type.name() + "' to '" + req_type.name() + "'.\n", -1);

  std::shared_ptr<Impl::holder<ConcreteType>> ptr = std::dynamic_pointer_cast<Impl::holder<ConcreteType>>(src.content());

  Errors::runtime_check(static_cast<bool>(ptr), "Error! Failed any_cast, since the stored pointer is null.\n", -1);

  return ptr->value();
}

} // namespace Homme

#endif // HOMMEXX_STD_META_HPP
