#pragma once
#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "torch/csrc/jit/assertions.h"
#include "torch/csrc/jit/source_range.h"
#include <torch/csrc/utils/memory.h>
#include <clocale>

namespace torch {
namespace jit {
namespace script {

// single character tokens are just the character itself '+'
// multi-character tokens need an entry here
// if the third entry is not the empty string, it is used
// in the lexer to match this token.

// These kinds are also used in Tree.h as the kind of the AST node.
// Some kinds TK_APPLY, TK_LIST are only used in the AST and are not seen in the
// lexer.

#define TC_FORALL_TOKEN_KINDS(_)                 \
  _(TK_EOF, "eof", "")                           \
  _(TK_WHITESPACE, "whitespace", "")             \
  _(TK_NUMBER, "number", "")                     \
  _(TK_NEWLINE, "newline", "")                   \
  _(TK_INDENT, "indent", "")                     \
  _(TK_DEDENT, "dedent", "")                     \
  _(TK_DEF, "def", "def")                        \
  _(TK_EQUIVALENT, "equivalent", "<=>")          \
  _(TK_IDENT, "ident", "")                       \
  _(TK_STRING, "string", "")                     \
  _(TK_STRINGLITERAL, "string_literal", "")      \
  _(TK_CONST, "const", "")                       \
  _(TK_LIST, "list", "")                         \
  _(TK_OPTION, "option", "")                     \
  _(TK_APPLY, "apply", "")                       \
  _(TK_COMPREHENSION, "comprehension", "")       \
  _(TK_RANGE_CONSTRAINT, "range_constraint", "") \
  _(TK_PARAM, "param", "")                       \
  _(TK_INFERRED, "inferred", "")                 \
  _(TK_ACCESS, "access", "")                     \
  _(TK_ASSIGN, "assign", "")                     \
  _(TK_ATTRIBUTE, "attribute", "")               \
  _(TK_IF, "if", "if")                           \
  _(TK_ELSE, "else", "else")                     \
  _(TK_ELIF, "elif", "elif")                     \
  _(TK_WHILE, "while", "while")                  \
  _(TK_EXPR_STMT, "expression statement", "")    \
  _(TK_RETURN, "return", "return")               \
  _(TK_IS, "is", "is")                           \
  _(TK_ISNOT, "is not", "is not")                \
  _(TK_NE, "ne", "!=")                           \
  _(TK_EQ, "eq", "==")                           \
  _(TK_LE, "le", "<=")                           \
  _(TK_GE, "ge", ">=")                           \
  _(TK_IF_EXPR, "if", "")                        \
  _(TK_TRUE, "True", "True")                     \
  _(TK_FALSE, "False", "False")                  \
  _(TK_NONE, "None", "None")                     \
  _(TK_AND, "and", "and")                        \
  _(TK_OR, "or", "or")                           \
  _(TK_NOT, "not", "not")                        \
  _(TK_CAST, "cast", "")                         \
  _(TK_PLUS_EQ, "+=", "+=")                      \
  _(TK_MINUS_EQ, "-=", "-=")                     \
  _(TK_TIMES_EQ, "*=", "*=")                     \
  _(TK_DIV_EQ, "/=", "/=")                       \
  _(TK_GLOBAL, "global", "global")               \
  _(TK_BUILT_IN, "built-in", "")                 \
  _(TK_SUBSCRIPT, "subscript", "")               \
  _(TK_VAR, "variable", "")                      \
  _(TK_NOTHING, "nothing", "")                   \
  _(TK_LIST_LITERAL, "list-literal", "")         \
  _(TK_TUPLE_LITERAL, "tuple-literal", "")       \
  _(TK_FOR, "for", "for")                        \
  _(TK_IN, "in", "in")                           \
  _(TK_STARRED, "starred", "")                   \
  _(TK_UNARY_MINUS, "unary minus", "")           \
  _(TK_POW, "pow operator", "**")                \
  _(TK_ARROW, "arrow", "->")                     \
  _(TK_DECL, "decl", "")                         \
  _(TK_SLICE_EXPR, "slice expr", "")             \
  _(TK_TYPE_COMMENT, "type comment", "# type:")  \
  _(TK_RAISE, "raise", "raise")                  \
  _(TK_ASSERT, "assert", "assert")


static const char* valid_single_char_tokens = "+-*/%@()[]:,={}><.?!";

enum TokenKind {
  // we use characters to represent themselves so skip all valid characters
  // before
  // assigning enum values to multi-char tokens.
  TK_DUMMY_START = 256,
#define DEFINE_TOKEN(tok, _, _2) tok,
  TC_FORALL_TOKEN_KINDS(DEFINE_TOKEN)
#undef DEFINE_TOKEN
};

std::string kindToString(int kind);
int stringToKind(std::string str);

// nested hash tables that indicate char-by-char what is a valid token.
struct TokenTrie;
using TokenTrieRef = std::unique_ptr<TokenTrie>;
struct TokenTrie {
  TokenTrie() : kind(0) {}
  void insert(const char* str, int tok) {
    if (*str == '\0') {
      JIT_ASSERT(kind == 0);
      kind = tok;
      return;
    }

    for (size_t i = 0, e = child_chars.size(); i < e; ++i) {
      if (child_chars[i] == *str) {
        child_tries[i]->insert(str + 1, tok);
        return;
      }
    }

    child_chars.emplace_back(*str);
    child_tries.emplace_back(torch::make_unique<TokenTrie>());
    child_tries.back()->insert(str + 1, tok);
  }
  int kind; // 0 == invalid token

  std::vector<char> child_chars;
  std::vector<TokenTrieRef> child_tries;
};

// stuff that is shared against all TC lexers/parsers and is initialized only
// once.
struct SharedParserData {
  SharedParserData() : head(new TokenTrie()) {
    std::stringstream ss;
    for (const char* c = valid_single_char_tokens; *c; c++) {
      std::string str(1, *c);
      head->insert(str.c_str(), *c);
    }

#define ADD_CASE(tok, _, tokstring) \
  if (*(tokstring) != '\0') {         \
    head->insert((tokstring), (tok));   \
  }
    TC_FORALL_TOKEN_KINDS(ADD_CASE)
#undef ADD_CASE
  }
#ifdef _WIN32
  double strtod_c(const char * str, char** end) {
    /// NOLINTNEXTLINE(hicpp-signed-bitwise)
    static _locale_t loc = _create_locale(LC_ALL, "C");
    return _strtod_l(str, end, loc);
  }
#else
  double strtod_c(const char * str, char** end) {
    /// NOLINTNEXTLINE(hicpp-signed-bitwise)
    static locale_t loc = newlocale(LC_ALL_MASK, "C", nullptr);
    return strtod_l(str, end, loc);
  }
#endif
  // 1. skip whitespace
  // 2. handle comment or newline
  //
  bool isNumber(const std::string& str, size_t start, size_t* len) {
    char first = str[start];
    // strtod allows numbers to start with + or - or nan or inf
    // http://en.cppreference.com/w/cpp/string/byte/strtof
    // but we want only the number part, otherwise 1+3 will turn into two
    // adjacent numbers in the lexer
    if (first == '-' || first == '+' || isalpha(first))
      return false;
    const char* startptr = str.c_str() + start;
    char* endptr;
    strtod_c(startptr, &endptr);
    *len = endptr - startptr;
    return *len > 0;
  }

  bool isCharCount(char c, const std::string& str, size_t start, int len) {
    //count checks from [start, start + len)
    return start + len <= str.size() && std::count(str.begin() + start, str.begin() + start + len, c) == len;
  }

  // python concatenates all adjacent strings "a" "b" == "ab"
  // strings can be enclosed with 1 or 3 single or double quotes
  // if enclosed with 3 quotes newlines are valid
  // as elsewhere, backslash and new line should be ignored
  bool isString(const std::string& str, size_t start, size_t* len) {
    char quote = str[start];
    if (quote != '\"' && quote != '\'')
      return false;
    int quote_len = isCharCount(quote, str, start, 3) ? 3 : 1;

    //end is now set past the opening quotation marks
    size_t end = start + quote_len;
    while(end < str.size() && !isCharCount(quote, str, end, quote_len)) {
      if (str[end] == '\n' && quote_len != 3) {
        return false;
      }
      //handle escaped characters. advances past escaped quotation marks,
      //escaped newlines and escaped backslashes
      if (str[end] == '\\') {
        end++;
      }
      end++;
    }
    //set length equal to the complete string including quotations
    *len = end - start + quote_len;
    //if end finished without going past the last character of the string than
    //there is a match
    return end < str.size();
  }

  bool isblank(int n) {
    return isspace(n) && n != '\n';
  }
  // Make an exception ignoring comments for type annotation comments
  bool isTypeComment(const std::string& str, size_t pos) {
    const std::string type_string = "# type:";
    if (str.size() < pos + type_string.length()) {
      return false;
    }
    auto match_string = str.substr(pos, type_string.size());
    return match_string == type_string;
  }
  // find the longest match of str.substring(pos) against a token, return true
  // if successful
  // filling in kind, start,and len
  bool match(
      const std::string& str,
      size_t pos,
      bool continuation, // are we inside a scope where newlines don't count
                         // (e.g. inside parens)
      bool whitespace_token, // should we treat whitespace as a token
      int* kind,
      size_t* start,
      size_t* len) {
    *start = pos;
    // skip whitespace
    while (pos < str.size() && isblank(str[pos]))
      pos++;

    // special handling
    if (pos < str.size()) {
      if (str[pos] == '#' && !isTypeComment(str, pos)) {
        // skip comments
        while (pos < str.size() && str[pos] != '\n')
          pos++;
        // tail call, handle whitespace and more comments
        return match(
            str, pos, continuation, whitespace_token, kind, start, len);
      }
      if (str[pos] == '\\' && pos + 1 < str.size() && str[pos + 1] == '\n' &&
          !whitespace_token) {
        return match(str, pos + 2, continuation, false, kind, start, len);
      }
      if (str[pos] == '\n') {
        return match(
            str, pos + 1, continuation, !continuation, kind, start, len);
      }
    }
    if (pos == str.size()) {
      *kind = TK_EOF;
      *start = pos;
      *len = 0;
      return true;
    }
    // invariant: the next token is not whitespace or newline
    if (whitespace_token) {
      *kind = TK_WHITESPACE;
      *len = pos - *start;
      return true;
    }
    *start = pos;
    // check for a valid number
    if (isNumber(str, pos, len)) {
      *kind = TK_NUMBER;
      return true;
    }
    // check for string
    if (isString(str, pos, len)) {
      *kind = TK_STRINGLITERAL;
      return true;
    }

    // check for either an ident or a token
    // ident tracks whether what we have scanned so far could be an identifier
    // matched indicates if we have found any match.
    bool matched = false;
    bool ident = true;
    TokenTrie* cur = head.get();
    for (size_t i = 0; pos + i < str.size() && (ident || cur != nullptr); i++) {
      ident = ident && validIdent(i, str[pos + i]);
      if (ident) {
        matched = true;
        *len = i + 1;
        *kind = TK_IDENT;
      }
      // check for token second, so that e.g. 'max' matches the token TK_MAX
      // rather the
      // identifier 'max'
      if (cur) {
        size_t child_offset = 0;
        for (size_t e = cur->child_chars.size(); child_offset < e; ++child_offset) {
          if (cur->child_chars[child_offset] == str[pos + i])
          break;
        }

        cur = (child_offset == cur->child_chars.size())
          ? nullptr
          : cur->child_tries[child_offset].get();

        if (cur && cur->kind != 0) {
          matched = true;
          *len = i + 1;
          *kind = cur->kind;
        }
      }
    }
    return matched;
  }
  bool isUnary(int kind, int* prec);
  bool isBinary(int kind, int* prec);
  bool isRightAssociative(int kind) {
    switch (kind) {
      case '?':
      case TK_POW:
        return true;
      default:
        return false;
    }
  }

 private:
  bool validIdent(size_t i, char n) {
    return isalpha(n) || n == '_' || (i > 0 && isdigit(n));
  }
  TokenTrieRef head;
};

SharedParserData& sharedParserData();

struct Token {
  int kind;
  SourceRange range;
  Token(int kind, const SourceRange& range) : kind(kind), range(range) {}
  std::string text() {
    return range.text();
  }
  std::string kindString() const {
    return kindToString(kind);
  }
};

struct Lexer {
  explicit Lexer(const std::string& str)
      : file(std::make_shared<std::string>(str)),
        pos(0),
        nesting(0),
        indent_stack(),
        next_tokens(),
        shared(sharedParserData()) {
    auto first_indent = lexRaw(true);
    indent_stack.push_back(first_indent.range.size());
    lex();
  }
  // Return the current token, and then move to the next one
  Token next() {
    if (next_tokens.size() == 0)
      reportError("Lexer invariant violated: empty token queue");
    Token r = next_tokens.front();
    next_tokens.erase(next_tokens.begin());
    if (next_tokens.size() == 0) {
      lex();
    }
    return r;
  }
  // Skip the current token if it matches the given kind
  bool nextIf(int kind) {
    if (cur().kind != kind)
      return false;
    next();
    return true;
  }

  [[noreturn]] void reportError(const std::string& what) {
    reportError(what, cur());
  }[[noreturn]] void reportError(const std::string& what, const Token& t) {
    std::stringstream ss;
    ss << what << ":\n";
    t.range.highlight(ss);
    throw std::runtime_error(ss.str());
  }
  [[noreturn]] void expected(const std::string& what, const Token& t) {
    std::stringstream ss;
    ss << "expected " << what << " but found '" << t.kindString()
       << "' here:\n";
    t.range.highlight(ss);
    throw std::runtime_error(ss.str());
  }[[noreturn]] void expected(const std::string& what) {
    expected(what, cur());
  }
  // Check that the current token has a given kind, return the current token,
  // and advance to the next one.
  Token expect(int kind) {
    if (cur().kind != kind) {
      expected(kindToString(kind));
    }
    return next();
  }
  Token& lookahead() {
    if (next_tokens.size() < 2) {
      lex();
    }
    return next_tokens[1];
  }
  Token& cur() {
    return next_tokens.front();
  }

 private:
  void lex() {
    auto r = lexRaw();
    switch (r.kind) {
      case '(':
      case '[':
      case '{':
        nesting++;
        break;
      case ')':
      case ']':
      case '}':
        nesting--;
        break;
      case TK_WHITESPACE: {
        int depth = r.range.size();
        if (depth > indent_stack.back()) {
          indent_stack.push_back(depth);
          r.kind = TK_INDENT;
        } else if (depth == indent_stack.back()) {
          r.kind = TK_NEWLINE;
        } else {
          next_tokens.emplace_back(TK_NEWLINE, r.range);
          while (indent_stack.back() != depth) {
            indent_stack.pop_back();
            next_tokens.emplace_back(TK_DEDENT, r.range);
            if (indent_stack.size() == 0) {
              reportError("invalid ident level", r);
            }
          }
          return; // We've already queued the tokens
        }
      } break;
      case TK_EOF:
        if (indent_stack.size() > 1) {
          next_tokens.emplace_back(TK_NEWLINE, r.range);
          next_tokens.emplace_back(TK_DEDENT, r.range);
          indent_stack.pop_back();
          return;
        }
        break;
      default:
        break;
    }
    next_tokens.push_back(std::move(r));
  }
  Token lexRaw(bool whitespace_token = false) {
    int kind;
    size_t start;
    size_t length;
    JIT_ASSERT(file);
    if (!shared.match(
            *file,
            pos,
            nesting > 0,
            whitespace_token,
            &kind,
            &start,
            &length)) {
      expected(
          "a valid token",
          Token((*file)[start], SourceRange(file, start, start + 1)));
    }
    auto t = Token(kind, SourceRange(file, start, start + length));
    pos = start + length;
    return t;
  }

  std::shared_ptr<std::string> file;
  size_t pos;
  size_t nesting; // depth of ( [ { nesting...
  std::vector<int> indent_stack; // stack of identation level of blocks
  // Invariant: this should always contain at least a single element
  std::vector<Token> next_tokens;
  SharedParserData& shared;
};
} // namespace script
} // namespace jit
} // namespace torch
