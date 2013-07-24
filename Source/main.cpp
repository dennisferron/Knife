
#include <iostream>
#include <string>
#include <sstream>
using namespace std;


#include "LikeMagic/Utility/TypeDescr.hpp"
using namespace LikeMagic;
using namespace LikeMagic::Utility;

#include "boost/spirit/include/qi.hpp"
#include "boost/fusion/include/io.hpp"
#include "boost/spirit/include/karma.hpp"

#include "boost/spirit/include/phoenix_core.hpp"
#include "boost/spirit/include/phoenix_operator.hpp"
#include "boost/spirit/include/phoenix_object.hpp"
#include "boost/fusion/include/adapt_struct.hpp"

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

namespace Syntax {

struct TupleExpr;
struct Invocation;
struct LabelExpr;
struct DefExpr;
struct BracesBlock;
struct Number;
struct QuotedString;
struct Reassignment;

typedef  boost::variant
<
    boost::recursive_wrapper<TupleExpr>,
    boost::recursive_wrapper<LabelExpr>,
    boost::recursive_wrapper<BracesBlock>,
    boost::recursive_wrapper<DefExpr>,
    boost::recursive_wrapper<Invocation>,
    boost::recursive_wrapper<Number>,
    boost::recursive_wrapper<QuotedString>
> Expr;

typedef  boost::variant
<
    boost::recursive_wrapper<Expr>,
    boost::recursive_wrapper<Reassignment>
> Stmt;

template <typename T>
struct DebugTrack
{
    char const* type;

    DebugTrack()
        : type(typeid(T).name())
    {
        //cout << type << " " << this << " constructed" << endl;
    }

    ~DebugTrack()
    {
        //cout << type << " " << this << " destructed" << endl;
    }
};

struct Ident : private DebugTrack<Ident>
{
    std::string value;
};

struct BracesBlock : private DebugTrack<BracesBlock>
{
    std::vector<Syntax::Stmt> stmts;
};

struct TupleExpr : private DebugTrack<TupleExpr>
{
    std::vector<Expr> elements;
};

struct LabelAssignment : private DebugTrack<LabelAssignment>
{
    Expr value;
};

struct LabelExpr : private DebugTrack<LabelExpr>
{
    boost::optional<Ident> name;
    boost::optional<Expr> type;
    boost::optional<LabelAssignment> term;
};

struct DefExpr : private DebugTrack<DefExpr>
{
    boost::optional<Ident> name;
    boost::optional<TupleExpr> args;
    BracesBlock code;
};

struct Invocation : private DebugTrack<Invocation>
{
    Ident name;
    boost::optional<TupleExpr> args;
    boost::optional<BracesBlock> postfix_lambda;
    boost::optional<
        boost::recursive_wrapper<
            Invocation
        >
    > next_call;
};

struct Number : private DebugTrack<Number>
{
    std::string raw;
};

struct QuotedString : private DebugTrack<QuotedString>
{
    std::string raw;
};

struct Reassignment : private DebugTrack<QuotedString>
{
    Ident name;
    Expr value;
};

}

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::Ident,
    (std::string, value)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::TupleExpr,
    (std::vector<Syntax::Expr>, elements)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::BracesBlock,
    (std::vector<Syntax::Stmt>, stmts)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::LabelAssignment,
    (Syntax::Expr, value)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::LabelExpr,
    (boost::optional<Syntax::Ident>, name)
    (boost::optional<Syntax::Expr>, type)
    (boost::optional<Syntax::LabelAssignment>, term)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::DefExpr,
    (boost::optional<Syntax::Ident>, name)
    (boost::optional<Syntax::TupleExpr>, args)
    (Syntax::BracesBlock, code)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::Invocation,
    (Syntax::Ident, name)
    (boost::optional<Syntax::TupleExpr>, args)
    (boost::optional<Syntax::BracesBlock>, postfix_lambda)
    (boost::optional<
        boost::recursive_wrapper<
            Syntax::Invocation
        >
    >, next_call)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::Number,
    (std::string, raw)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::QuotedString,
    (std::string, raw)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::Reassignment,
    (Syntax::Ident, name)
    (Syntax::Expr, value)
)

struct printer
{
    typedef boost::spirit::utf8_string string;

    void element(string const& tag, string const& value, int depth) const
    {
        for (int i = 0; i < (depth*4); ++i) // indent to depth
            std::cout << ' ';

        std::cout << "tag: " << tag;
        if (value != "")
            std::cout << ", value: " << value;
        std::cout << std::endl;
    }
};

void print_info(boost::spirit::info const& what)
{
    using boost::spirit::basic_info_walker;

    printer pr;
    basic_info_walker<printer> walker(pr, what.tag, 0);
    boost::apply_visitor(walker, what.value);
}

template <typename Iterator>
struct Skipper : qi::grammar<Iterator>
{
    Skipper() : Skipper::base_type(start)
    {
        start = +qi::char_(" \t");
    }

    qi::rule<Iterator> start;
};

template <typename Iterator>
struct LangParseGrammar : qi::grammar<Iterator, Syntax::DefExpr(), Skipper<Iterator>>
{
    LangParseGrammar() : LangParseGrammar::base_type(def_expr)
    {
        ident = qi::lexeme[qi::alpha >> *(qi::alnum | qi::char_('_') )];
        label = -ident >> qi::lit(":");
        label_assignment = qi::lit("=") > expr;
        label_expr = label >> -expr >> -label_assignment;
        expr_list = expr % qi::lit(",");
        tuple_expr = expr_list;
        paren_arg_list %= qi::lit("(") > -tuple_expr > qi::lit(")");
        braces_block = qi::lit("{") > stmt_list > qi::lit("}");
        def_expr = qi::lit("def") > -ident > -paren_arg_list > braces_block;
        invocation = ident >> -paren_arg_list >> -braces_block >> -invocation;
        number_str %= qi::lexeme[+qi::digit];
        number = number_str;
        quoted_string = qi::lexeme[qi::lit('"') > *(qi::char_-'"') > '"'];
        paren_expr %= qi::lit('(') > expr > ')';
        reassignment = ident >> qi::lit("=") > expr;
        stmt = expr | reassignment;
        expr = def_expr | label_expr | paren_expr | braces_block | invocation | number | quoted_string | paren_expr;
        stmt_list = expr % +qi::char_("\n;");
    }

    qi::rule<Iterator, std::string(), Skipper<Iterator>> dummy_str;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> ident;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> number_str;
    qi::rule<Iterator, Syntax::Ident(), Skipper<Iterator>> label;
    qi::rule<Iterator, Syntax::LabelAssignment(), Skipper<Iterator>> label_assignment;
    qi::rule<Iterator, Syntax::LabelExpr(), Skipper<Iterator>> label_expr;
    qi::rule<Iterator, Syntax::Expr(), Skipper<Iterator>> expr;
    qi::rule<Iterator, Syntax::TupleExpr(), Skipper<Iterator>> tuple_expr;
    qi::rule<Iterator, Syntax::DefExpr(), Skipper<Iterator>> def_expr;
    qi::rule<Iterator, Syntax::TupleExpr(), Skipper<Iterator>> paren_arg_list;
    qi::rule<Iterator, Syntax::BracesBlock(), Skipper<Iterator>> braces_block;
    qi::rule<Iterator, Syntax::Number(), Skipper<Iterator>> number;
    qi::rule<Iterator, Syntax::QuotedString(), Skipper<Iterator>> quoted_string;
    qi::rule<Iterator, Syntax::Reassignment(), Skipper<Iterator>> reassignment;
    qi::rule<Iterator, Syntax::Stmt(), Skipper<Iterator>> stmt;
    qi::rule<Iterator, std::vector<Syntax::Stmt>(), Skipper<Iterator>> stmt_list;
    qi::rule<Iterator, Syntax::Invocation(), Skipper<Iterator>> invocation;
    qi::rule<Iterator, Syntax::Expr(), Skipper<Iterator>> paren_expr;
    qi::rule<Iterator, std::vector<Syntax::Expr>(), Skipper<Iterator>> expr_list;
};


Syntax::DefExpr Parse(std::string str)
{
    typedef std::string::const_iterator iterator_type;
    typedef LangParseGrammar<iterator_type> LangGrammar;

    LangGrammar g; // Our grammar

    std::string::const_iterator iter = str.begin();
    std::string::const_iterator end = str.end();
    Skipper<iterator_type> skipper;
    Syntax::DefExpr result;

    try
    {
        bool success = phrase_parse(iter, end, g, skipper, result);

        if (!success)
        {
            stringstream ss;
            ss << "LangParser failed to parse: " << str << std::endl;
            cerr << endl << ss.str() << endl;
            //raiseError(ParseException(ss.str()));
        }
        else
        {
            if (iter != end)
            {
                stringstream ss;
                ss << "Not all of the line was parsed: " << std::string(iter, end) << std::endl;
                cerr << endl << ss.str() << endl;
                //raiseError(ParseException(ss.str()));
            }
        }

        return result;
    }
    catch (qi::expectation_failure<iterator_type> const& x)
    {
        std::cout << "expected: "; print_info(x.what_);
        std::cout << "got: \"" << std::string(x.first, x.last) << '"' << std::endl;
        throw;
    }
}

struct SyntaxPrinter : boost::static_visitor<std::string>
{
    std::string operator()(const Syntax::Invocation& t) const
    {
        auto self = *this;
        std::stringstream result;
        result << self(t.name);
        result << "(" << self(t.args) << ")";
        result << self(t.postfix_lambda);
        result << self(t.next_call);
        return result.str();
    }

    std::string operator()(const Syntax::DefExpr& t) const
    {
        auto self = *this;
        std::stringstream result;
        result << "def ";
        result << self(t.name);
        result << "(" << self(t.args) << ")";
        result << endl;
        result << self(t.code) << endl;
        return result.str();
    }

    std::string operator()(const Syntax::BracesBlock& t) const
    {
        auto self = *this;
        std::stringstream result;

        result << "{";

        for (auto expr : t.stmts)
        {
            result << endl << self(expr);
        }

        result << endl << "}";

        return result.str();
    }

    std::string operator()(const Syntax::TupleExpr& t) const
    {
        auto self = *this;
        std::stringstream result;

        bool is_first = true;
        for (auto expr : t.elements)
        {
            if (!is_first)
                result << ", ";

            result << self(expr);

            is_first = false;
        }

        return result.str();
    }

    std::string operator()(const Syntax::Ident& t) const
    {
        return t.value;
    }

    template <typename T>
    std::string operator()(boost::optional<T> const& t)
    {
        if (t)
            return this->operator()(*t);
        else
            return "_";
    }

    template <typename T>
    std::string operator()(boost::recursive_wrapper<T> const& t)
    {
        return this->operator()(t.get());
    }

    std::string operator()(Syntax::Expr const& t)
    {
        return boost::apply_visitor(*this, t);
    }

    std::string operator()(Syntax::Stmt const& t)
    {
        return boost::apply_visitor(*this, t);
    }

    std::string operator()(Syntax::LabelExpr const& t)
    {
        auto self = *this;
        stringstream result;
        result << self(t.name);
        result << ":" << self(t.type);
        result << self(t.term);
        return result.str();
    }

    std::string operator()(Syntax::Reassignment const& t)
    {
        auto self = *this;
        stringstream result;
        result << self(t.name);
        result << "=" << self(t.value);
        return result.str();
    }

    std::string operator()(Syntax::LabelAssignment const& t)
    {
        return "=" + operator()(t.value);
    }

    std::string operator()(Syntax::Number const& t)
    {
        return t.raw;
    }

    template <typename T>
    std::string operator()(const T& t) const
    {
        return TypeDescr<T>::text();
    }
};

namespace Semantic
{

class Expr
{
public:
    virtual Expr force() = 0;
};

class DefSpec
{
private:
    std::vector<Stmt> code;
public:
    Expr call(std::vector<Expr> args);
};

class Deck
{
private:
    std::string name;
    std::vector<DefSpec> specs;
};

}

// Maybe the constructor should be a template rather than the whole class?
template <typename T>
struct ParseState
{
    T value;
    boost::unordered_map<std::string, Semantic::Deck> decks;

    ParseState<Semantic::Deck> parse(Syntax::DefExpr def_expr)
    {
        // TODO: Build new decks based on this.
        auto deck = decks.get_or_add(def_expr.name, &[](std::string key){ return Semantic::Deck(key) });
        return ParseState<Semantic::Deck>(this, deck);
    }
};

int main(int argc, char* argv[])
{
    auto result = Parse("def Main(x:Int=0, y:) { if(x) { say(1) } else { say(2) } }");
    std::cout << boost::apply_visitor(SyntaxPrinter(), result) << std::endl;
    std::cout << "Press enter..." << std::endl;
    std::cin.ignore( 99, '\n' );
    return 0;
}
