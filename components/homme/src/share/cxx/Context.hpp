/********************************************************************************
 * HOMMEXX 1.0: Copyright of Sandia Corporation
 * This software is released under the BSD license
 * See the file 'COPYRIGHT' in the HOMMEXX/src/share/cxx directory
 *******************************************************************************/

#ifndef HOMMEXX_CONTEXT_HPP
#define HOMMEXX_CONTEXT_HPP

#include <string>
#include <map>
#include <memory>

#include "ErrorDefs.hpp"
#include "utilities/StdMeta.hpp"

namespace Homme {

/* A Context manages resources that are morally singletons. Context is
 * meant to have two roles. First, a Context singleton is the only singleton in
 * the program. Second, a context need not be a singleton, and each Context
 * object can have different Elements, ReferenceElement, etc., objects. (That
 * probably isn't needed, but Context immediately supports it.)
 *
 * Finally, Context has two singleton functions: singleton(), which returns
 * Context&, and finalize_singleton(). The second is called in a unit test exe
 * main before Kokkos::finalize().
 */
class Context {
public:

  // Adding a context member passing its construction args
  template<typename ConcreteType, typename... Args>
  void build_and_add (Args... args);

  // Adding a context member passing a shared_ptr
  template<typename ConcreteType>
  void add (std::shared_ptr<ConcreteType> member);

  // Getters for a managed object.
  template<typename ConcreteType>
  bool has () const;

  // Getters for a managed object.
  template<typename ConcreteType>
  ConcreteType& get ();

  // Exactly one singleton.
  static Context& singleton();

  static void finalize_singleton();
private:

  std::map<std::string,Homme::any> m_members;

  // Clear the objects Context manages.
  void clear();
};

// ==================== IMPLEMENTATION =================== //

template<typename ConcreteType, typename... Args>
void Context::build_and_add (Args... args) {
  std::shared_ptr<ConcreteType> ptr = std::make_shared<ConcreteType>(args...);
  add(ptr);
}

template<typename ConcreteType>
void Context::add (std::shared_ptr<ConcreteType> member) {
  const std::string& name = typeid(ConcreteType).name();

  auto it = m_members.find(name);
  Errors::runtime_check(it==m_members.end(), "Error! Context member '" + name + "' was already added to the context.\n", -1);

  m_members.emplace(name,member);
}

template<typename ConcreteType>
bool Context::has () const {
  const std::string& name = typeid(ConcreteType).name();
  auto it = m_members.find(name);
  return it!=m_members.end();
}

template<typename ConcreteType>
ConcreteType& Context::get () {
  if (!has<ConcreteType>()) {
    build_and_add<ConcreteType>();
  }

  const std::string& name = typeid(ConcreteType).name();
  auto it = m_members.find(name);
  Errors::runtime_check(it!=m_members.end(), "Error! Context member '" + name + "' not found. This is an internal bug. Please, contact developers.\n", -1);

  return any_cast<ConcreteType>(it->second);
}

} // namespace Homme

#endif // HOMMEXX_CONTEXT_HPP
