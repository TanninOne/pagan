#pragma once

#include <tao/pegtl.hpp>
#include <tao/pegtl/analyze.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>

#include <any>
#include <functional>
#include <iostream>
#include <map>
#include <vector>

#include "IScriptQuery.h"
#include "flexi_cast.h"
#include "format.h"

namespace pegtl = tao::TAO_PEGTL_NAMESPACE;

typedef std::function<std::any(const std::any& args)> AnyFunc;

template <typename T>
bool equal(const std::any &lhs, const std::any &rhs) {
  return flexi_cast<T>(lhs) == flexi_cast<T>(rhs);
}

static bool strEqual(const std::any &lhs, const std::any &rhs) {
  return strcmp(flexi_cast<std::string>(lhs).c_str(), flexi_cast<std::string>(rhs).c_str()) == 0;
}

static const inline std::map<std::type_index, std::function<bool(const std::any &lhs, const std::any &rhs)>> comparators{
  { typeid(char), equal<char> },
  { typeid(short), equal<short> },
  { typeid(int), equal<int> },
  { typeid(long), equal<long> },
  { typeid(int8_t), equal<int8_t> },
  { typeid(int16_t), equal<int16_t> },
  { typeid(int32_t), equal<int32_t> },
  { typeid(int64_t), equal<int64_t> },
  { typeid(uint8_t), equal<uint8_t> },
  { typeid(uint16_t), equal<uint16_t> },
  { typeid(uint32_t), equal<uint32_t> },
  { typeid(uint64_t), equal<uint64_t> },
  { typeid(std::string), strEqual },
};


static inline bool any_equal(const std::any &lhs, const std::any &rhs) {
  try {
    return comparators.at(lhs.type())(lhs, rhs);
  }
  catch (const std::out_of_range &) {
    return false;
  }
  catch (const std::bad_any_cast &) {
    return false;
  }
}

struct Rearrange : std::true_type {
  template<typename... States>
  static void transform(std::unique_ptr<pegtl::parse_tree::node> &node, States &&...st) {
    if (node->children.size() == 1) {
      node = std::move(node->children.back());
    } else if (node->children.size() == 0) {
      return;
    } else {
      node->remove_content();
      auto &children = node->children;
      auto rhs = std::move(children.back());
      children.pop_back();
      auto op = std::move(children.back());
      children.pop_back();
      op->children.emplace_back(std::move(node));
      op->children.emplace_back(std::move(rhs));
      node = std::move(op);
      transform(node->children.front(), st...);
    }
  }
};

namespace ExpressionSpec {

  enum class Order : int
  {
  };

  typedef std::function<std::any(const std::string &key, uint64_t id)> VariableResolver;
  typedef std::function<void(const std::string &key, const std::any &value)> VariableAssigner;

  struct Operation
  {
    Order order;
    std::function<std::any(std::any, std::any)> func;
  };

  struct Stack
  {
    void push(const Operation &op)
    {
      while (!m_Operations.empty() && (m_Operations.back().order <= op.order)) {
        reduce();
      }
      m_Operations.push_back(op);
    }

    void push(const std::any &value)
    {
      m_Values.push_back(value);
    }

    std::any result()
    {
      while (!m_Operations.empty()) {
        reduce();
      }

      std::any result = m_Values.back();
      m_Values.clear();
      return result;
    }

  private:
    std::vector<Operation> m_Operations;
    std::vector<std::any> m_Values;

    void reduce()
    {
      std::any rhs = m_Values.back();
      m_Values.pop_back();
      std::any lhs = m_Values.back();
      m_Values.pop_back();
      const Operation op = m_Operations.back();
      m_Operations.pop_back();
      m_Values.push_back(op.func(lhs, rhs));
    }
  };

  class Stacks {
  public:
    Stacks()
    {
      open();
    }

    void open()
    {
      m_Values.emplace_back();
    }

    template <typename T>
    void push(const T &value)
    {
      // value can be a value or an operator
      m_Values.back().push(value);
    }

    void close()
    {
      const auto res = m_Values.back().result();
      m_Values.pop_back();
      m_Values.back().push(res);
    }

    std::any finish()
    {
      return m_Values.back().result();
    }

  private:
    std::vector<Stack> m_Values;
  };

  class Operators
  {
  public:
    Operators()
    {
      insert("*", Order(5), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) * flexi_cast<int64_t>(rhs); });
      insert("/", Order(5), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) / flexi_cast<int64_t>(rhs); });
      insert("%", Order(5), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) % flexi_cast<int64_t>(rhs); });
      insert("+", Order(6), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) + flexi_cast<int64_t>(rhs); });
      insert("-", Order(6), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) - flexi_cast<int64_t>(rhs); });
      insert("<<", Order(7), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) << flexi_cast<int64_t>(rhs); });
      insert(">>", Order(7), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) >> flexi_cast<int64_t>(rhs); });
      insert("<", Order(8), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) < flexi_cast<int64_t>(rhs); });
      insert(">", Order(8), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) > flexi_cast<int64_t>(rhs); });
      insert("<=", Order(8), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) <= flexi_cast<int64_t>(rhs); });
      insert(">=", Order(8), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) >= flexi_cast<int64_t>(rhs); });
      insert("==", Order(9), [](const std::any& lhs, const std::any& rhs) { return any_equal(lhs, rhs); });
      insert("!=", Order(9), [](const std::any &lhs, const std::any &rhs) { return !any_equal(lhs, rhs); });
      insert("&", Order(10), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) & flexi_cast<int64_t>(rhs); });
      insert("^", Order(11), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) ^ flexi_cast<int64_t>(rhs); });
      insert("|", Order(12), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) | flexi_cast<int64_t>(rhs); });
      insert("&&", Order(13), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<bool>(lhs) && flexi_cast<bool>(rhs); });
      insert("and", Order(13), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<bool>(lhs) && flexi_cast<bool>(rhs); });
      insert("||", Order(14), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<bool>(lhs) || flexi_cast<bool>(rhs); });
      insert("or", Order(14), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<bool>(lhs) || flexi_cast<bool>(rhs); });
    }

    void insert(const std::string& name, const Order order, const std::function<std::any(std::any, std::any) > &func)
    {
      m_Operators.insert({ name,{ order, func } });
    }

    const std::map<std::string, Operation> &ops() const
    {
      return m_Operators;
    }

  private:
    std::map<std::string, Operation> m_Operators;
  };

  using namespace tao::pegtl;

  class Ignored : public sor<space> {};

  struct Infix
  {
    using analyze_t = analysis::generic<analysis::rule_type::ANY>;

    template<apply_mode,
      rewind_mode,
      template<typename...> class Action,
      template<typename...> class Control,
      typename Input
    >
      static bool match(Input &input, const Operators &ops/*, Stacks &stacks, const VariableResolver&*/)
    {
      // return matchImpl(input, ops, stacks);
      return matchImpl(input, ops);
    }

  private:
    template<typename Input>
    static bool matchImpl(Input& in, const Operators &ops/*, Stacks &stacks*/)
    {
      std::string t;
      size_t size = in.size();
      size_t offset = 0;
      // no operator is more than 2 characters long
      while ((offset < size) && (offset < 2)) {
        char ch = in.peek_char(offset);
        ++offset;
        if (ch == ' ') {
          break;
        }

        t.push_back(ch);
      }

      auto iter = ops.ops().lower_bound(t);
      while ((t.length() > 0) && ((iter == ops.ops().end()) || (iter->first != t))) {
        t.pop_back();
        iter = ops.ops().lower_bound(t);
      }

      if (t.length() > 0) {
        // stacks.push(iter->second);
        in.bump(t.size());
        return true;
      }
      else {
        return false;
      }
    }
  };

  struct Number : seq<opt<one<'+', '-'>>, plus<digit>> {};
  struct HexNumber : seq<one<'0'>, one<'x'>, plus<xdigit>> {};
  struct String : sor<seq<one<'"'>, star<not_one<'"'>>, one<'"'>>, seq<one<'\''>, star<not_one<'\''>>, one<'\''>>> {};
  struct Identifier : seq<sor<alpha, one<'_'>>, star<sor<alnum, one<'.'>, one<'_'>>>> {};
  struct Expression;
  struct Function;
  struct Not;
  struct Ternary;
  struct Bracket : if_must<one<'('>, Expression, one<')'>> {};
  struct Atomic : sor<HexNumber, Not, Number, String, Function, Identifier, Bracket> {};
  struct Ternary : seq<Expression, star<Ignored>, one<'?'>, star<Ignored>, Expression, star<Ignored>, one<':'>, star<Ignored>, Expression> {};
  struct Not : seq<sor<istring<'n', 'o', 't', ' '>, one<'!'>>, Atomic> {};
  struct Assignment : seq<Identifier, star<Ignored>, one<'='>, star<Ignored>, Expression> {};
  struct Function : seq<Identifier, one<'('>, Atomic, one<')'>> {};
  struct Expression : list<Atomic, Infix, Ignored> {};
  struct Grammar : must<sor<Assignment, Ternary, Expression>, eof> {};

  template<typename Rule>
  struct Action : pegtl::nothing<Rule> {};

  template <>
  struct Action<Not>
  {
    template<typename Input>
    static void apply(const Input& input, const Operators&, Stacks& stacks, const VariableResolver&)
    {
      stacks.push(!std::stol(input.string()));
    }
  };

  template <>
  struct Action<Number>
  {
    template<typename Input>
    static void apply(const Input &input, const Operators&, Stacks &stacks, const VariableResolver&)
    {
      stacks.push(std::stol(input.string()));
    }
  };

  template <>
  struct Action<HexNumber>
  {
    template<typename Input>
    static void apply(const Input &input, const Operators&, Stacks &stacks, const VariableResolver&)
    {
      stacks.push(std::stol(input.string(), nullptr, 16));
    }
  };

  template <>
  struct Action<String>
  {
    template<typename Input>
    static void apply(const Input &input, const Operators&, Stacks &stacks, const VariableResolver&)
    {
      std::string inString = input.string();
      stacks.push(inString.substr(1, inString.length() - 2));
    }
  };

  template <>
  struct Action<Identifier>
  {
    template<typename Input>
    static void apply(const Input &input, const Operators&, Stacks &stacks, const VariableResolver &variables)
    {
      stacks.push(variables(input.string()));
    }
  };

  template <>
  struct Action<one<'('>>
  {
    static void apply0(const Operators&, Stacks &stacks, const VariableResolver&)
    {
      stacks.open();
    }
  };

  template <>
  struct Action<one<')'>>
  {
    static void apply0(const Operators&, Stacks& stacks, const VariableResolver &)
    {
      stacks.close();
    }
  };

  template <typename Rule>
  using Selector = pegtl::parse_tree::selector<
    Rule,
    pegtl::parse_tree::apply_store_content::to<Number, Not, HexNumber, String, Identifier, Ternary, Function, Infix>,
    pegtl::parse_tree::apply_remove_content::to<>,
    pegtl::parse_tree::apply<Rearrange>::to<Expression>
  >;
}

static void printNode(const pegtl::parse_tree::node &node, const std::string &indent = "") {
  if (node.is_root()) {
    std::cout << "root \"" << node.content() << "\" at " << node.begin() << " to " << node.end() << " - " << node.is<ExpressionSpec::Not>() << std::endl;
  } else {
    if (node.has_content()) { 
      std::cout << indent << node.id->name() << " \"" << node.content() << "\" at " << node.begin() << " to " << node.end() << " - " << node.is<ExpressionSpec::Infix>() << std::endl;
    } else {
      std::cout << indent << node.id->name() << "\" at " << node.begin() << std::endl;
    }
  }

  if (!node.children.empty()) {
    auto nextIndent = indent + "  ";
    for (auto &ch : node.children) {
      printNode(*ch, nextIndent);
    }
  }
}

static std::map<std::string, std::function<std::any(const std::any&, const std::any&)>> operators = {
  { "*", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) * flexi_cast<int64_t>(rhs); } },
  { "/", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) / flexi_cast<int64_t>(rhs); }},
  { "%", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) % flexi_cast<int64_t>(rhs); }},
  { "+", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) + flexi_cast<int64_t>(rhs); }},
  { "-", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) - flexi_cast<int64_t>(rhs); }},
  { "<<", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) << flexi_cast<int64_t>(rhs); }},
  { ">>", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) >> flexi_cast<int64_t>(rhs); }},
  { "<", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) < flexi_cast<int64_t>(rhs); }},
  { ">", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) > flexi_cast<int64_t>(rhs); }},
  { "<=", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) <= flexi_cast<int64_t>(rhs); }},
  { ">=", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) >= flexi_cast<int64_t>(rhs); }},
  { "==", [](const std::any &lhs, const std::any &rhs) { return any_equal(lhs, rhs); }},
  { "!=", [](const std::any &lhs, const std::any &rhs) { return !any_equal(lhs, rhs); }},
  { "&", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) & flexi_cast<int64_t>(rhs); }},
  { "^", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) ^ flexi_cast<int64_t>(rhs); }},
  { "|", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) | flexi_cast<int64_t>(rhs); }},
  { "&&", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<bool>(lhs) && flexi_cast<bool>(rhs); }},
  { "||", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<bool>(lhs) || flexi_cast<bool>(rhs); }},
};


static std::any evalNode(const pegtl::parse_tree::node& node, const ExpressionSpec::VariableResolver& resolver, const ExpressionSpec::VariableAssigner& assigner, std::map<std::pair<uint64_t, uint64_t>, std::string> &identifiers) {
  if (node.is_root()) {
    std::any res = evalNode(**node.children.rbegin(), resolver, assigner, identifiers);
    if (node.children.size() == 2) {
      std::any varAny = evalNode(*node.children.front(), [](const std::string& key, uint64_t id) { return key; }, assigner, identifiers);
      std::string var = flexi_cast<std::string>(varAny);
      assigner(var, res);
    }

    return res;
  }

  std::pair<uint64_t, uint64_t> idKey(node.begin().byte, node.end().byte);
  auto id = identifiers.find(idKey);
  if (id == identifiers.end()) {
    identifiers[idKey] = node.content();
    id = identifiers.find(idKey);
  }

  if (node.is<ExpressionSpec::Infix>()) {
    const auto& iter = operators.find(id->second);

    std::any lhs = evalNode(*(node.children.at(0)), resolver, assigner, identifiers);
    std::any rhs = evalNode(*(node.children.at(1)), resolver, assigner, identifiers);
    return (iter->second)(lhs, rhs);
  } else if (node.is<ExpressionSpec::Ternary>()) {
    std::any lhs = evalNode(*(node.children.at(0)), resolver, assigner, identifiers);
    bool cond = std::any_cast<bool>(lhs);
    return evalNode(*(node.children.at(cond ? 1 : 2)), resolver, assigner, identifiers);
  } else if (node.is<ExpressionSpec::Function>()) {
    std::any arg = evalNode(*(node.children.at(1)), resolver, assigner, identifiers);
    AnyFunc func = std::any_cast<AnyFunc>(evalNode(*(node.children.at(0)), resolver, assigner, identifiers));
    return func(arg);
  } else if (node.is<ExpressionSpec::Identifier>()) {
    return resolver(id->second, reinterpret_cast<uint64_t>(node.m_begin.data));
  } else if (node.is<ExpressionSpec::HexNumber>()) {
    return strtol(id->second.c_str(), nullptr, 16);
  } else if (node.is<ExpressionSpec::Number>()) {
    return strtol(id->second.c_str(), nullptr, 10);
  } else if (node.is<ExpressionSpec::String>()) {
    return std::string(node.m_begin.data + 1, node.m_end.data - 1);
  } else if (node.is<ExpressionSpec::Not>()) {
    std::any result = evalNode(*(node.children.at(0)), resolver, assigner, identifiers);
    return !std::any_cast<bool>(result);
  } else {
    return node.content();
  }
}

std::vector<std::string> splitVariable(const std::string &input);

template <typename T>
std::function<T(const IScriptQuery &)> makeFuncImpl(const std::string &code) {
  ExpressionSpec::Operators operators;
  // TODO these are raw pointers that will never get cleaned. Since they are required in the
  // function we return, to clean up we'd have to wrap std::function I think, which probably isn't
  // worth it since we don't expect the function to be cleaned until the process ends
  pegtl::string_input<> *expressionString = new pegtl::string_input<>(code, "source");
  auto tree =
    pegtl::parse_tree::parse<ExpressionSpec::Grammar, ExpressionSpec::Selector>(*expressionString, operators).release();

  // printNode(*tree);

  std::map<uint64_t, std::vector<std::string>> variables;
  std::map<std::pair<uint64_t, uint64_t>, std::string> identifiers;

  return [tree, variables, identifiers](const IScriptQuery &obj) mutable -> T {
    ExpressionSpec::VariableResolver resolver = [&obj, &variables](const std::string &key, uint64_t id) -> std::any {
      auto varIter = variables.find(id);
      if (varIter == variables.end()) {
        variables.insert(std::pair<uint64_t, std::vector<std::string>>(id, splitVariable(key)));
        // variables[id] = splitVariable(key);
        varIter = variables.find(id);
        // varIter = variables.insert(std::make_pair(id, splitVariable(key))).first;
      }
      // ObjectIndex *idx = obj.getIndex();
      return obj.getAny(varIter->second.begin(), varIter->second.end());
    };

    ExpressionSpec::VariableAssigner assigner = [&obj, &variables](const std::string &key, const std::any &value) {
      throw std::runtime_error("attempt to assign in read-only function");
    };

    std::any res = evalNode(*tree, resolver, assigner, identifiers);
    return flexi_cast<T>(res);
  };
}

template <typename T>
std::function<T(IScriptQuery &, const std::any&)> makeFuncMutableImpl(const std::string &code) {
  ExpressionSpec::Operators operators;
  // TODO these are raw pointers that will never get cleaned. Since they are required in the
  // function we return, to clean up we'd have to wrap std::function I think, which probably isn't
  // worth it since we don't expect the function to be cleaned until the process ends
  pegtl::string_input<> *expressionString = new pegtl::string_input<>(code, "source");
  auto tree =
    pegtl::parse_tree::parse<ExpressionSpec::Grammar, ExpressionSpec::Selector>(*expressionString, operators).release();

  // printNode(*tree);

  static std::map<std::string, AnyFunc> functions = {
    { "length", [](const std::any& args) {
      try {
        return std::any_cast<std::string>(args).length();
      } catch (const std::bad_any_cast&) {
        return std::any_cast<std::vector<uint8_t>>(args).size();
      }
    } },
    { "size", [](const std::any& args) {
      return flexi_cast<std::string>(args).length() + 1;
    } },
  };

  std::map<uint64_t, std::vector<std::string>> variables;
  std::map<std::pair<uint64_t, uint64_t>, std::string> identifiers;

  return [tree, variables, identifiers](IScriptQuery &obj, const std::any& value) mutable -> T {
    ExpressionSpec::VariableResolver resolver = [&obj, &variables, &value](const std::string &key, uint64_t id) -> std::any {
      if (key == "value") {
        return value;
      }

      auto funcIter = functions.find(key);
      if (funcIter != functions.end()) {
        return funcIter->second;
      }

      auto varIter = variables.find(id);
      if (varIter == variables.end()) {
        variables.insert(std::pair<uint64_t, std::vector<std::string>>(id, splitVariable(key)));
        // variables[id] = splitVariable(key);
        varIter = variables.find(id);
        // varIter = variables.insert(std::make_pair(id, splitVariable(key))).first;
      }
      // ObjectIndex *idx = obj.getIndex();
      return obj.getAny(varIter->second.begin(), varIter->second.end());
    };

    ExpressionSpec::VariableAssigner assigner = [&obj, &variables](const std::string &key, const std::any &value) {
      std::vector<std::string> keySegments = splitVariable(key);
      // ObjectIndex *idx = obj.getIndex();
      return obj.setAny(keySegments.begin(), keySegments.end(), value);
    };

    std::any res = evalNode(*tree, resolver, assigner, identifiers);
    return flexi_cast<T>(res);
  };
}

template <typename T>
inline std::function<T(const IScriptQuery &)> makeFunc(const std::string &code) {
  try {
    return makeFuncImpl<T>(code);
  }
  catch (const std::exception& e) {
    throw std::runtime_error(fmt::format("failed to compile function \"{}\"", code).c_str());
  }
}

template <>
inline std::function<int32_t(const IScriptQuery &)> makeFunc(const std::string &code) {
  char *endPtr = nullptr;
  long num = std::strtol(code.c_str(), &endPtr, 10);
  if (*endPtr == '\0') {
    return [num](const IScriptQuery &) -> int32_t { return num; };
  }

  try {
    return makeFuncImpl<int32_t>(code);
  }
  catch (const std::exception& e) {
    throw std::runtime_error(fmt::format("failed to compile function \"{}\"", code).c_str());
  }
}

template <typename T>
inline std::function<T(IScriptQuery &, const std::any&)> makeFuncMutable(const std::string &code) {
  try {
    return makeFuncMutableImpl<T>(code);
  }
  catch (const std::exception& e) {
    throw std::runtime_error(fmt::format("failed to compile function \"{}\"", code).c_str());
  }
}
