#ifndef HOMMEXX_CONTEXT_MEMBER_BASE_HPP
#define HOMMEXX_CONTEXT_MEMBER_BASE_HPP

namespace Homme {

// A dummy base class for all objects stores in the Context
// This allows to store different objects in the same container.
// NOTE: with c++17 we could simply store a map of std::any,
//       without the need of this dummy base class.
class ContextMemberBase {
public:
  virtual ~ContextMemberBase () {}
};

} // namespace Homme

#endif // HOMMEXX_CONTEXT_MEMBER_BASE_HPP
