
#include <iostream>
#include <string>

using namespace std;

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
struct TypeExpr;
struct TermExpr;
struct LabelExpr;
struct DefExpr;

typedef  boost::variant
<
    boost::recursive_wrapper<TupleExpr>,
    boost::recursive_wrapper<TypeExpr>,
    boost::recursive_wrapper<TermExpr>,
    boost::recursive_wrapper<LabelExpr>,
    boost::recursive_wrapper<DefExpr>
> Expr;

template <typename T>
struct DebugTrack
{
    char const* type;

    DebugTrack()
        : type(typeid(T).name())
    {
        cout << type << " " << this << " constructed" << endl;
    }

    ~DebugTrack()
    {
        cout << type << " " << this << " destructed" << endl;
    }
};

struct LabelIdent : private DebugTrack<LabelIdent>
{
    std::string value;
};

struct DefIdent : private DebugTrack<DefIdent>
{
    std::string value;
};

struct SlotIdent : private DebugTrack<SlotIdent>
{
    std::string value;
};

struct TypeExpr : private DebugTrack<TypeExpr>
{
    std::string raw;
};

struct TermExpr : private DebugTrack<TermExpr>
{
    std::string raw;
};

struct BracesBlock : private DebugTrack<BracesBlock>
{
    std::vector<Syntax::Expr> stmts;
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
    boost::optional<LabelIdent> name;
    boost::optional<TypeExpr> type;
    boost::optional<LabelAssignment> term;
};

struct DefExpr : private DebugTrack<DefExpr>
{
    boost::optional<DefIdent> name;
    boost::optional<TupleExpr> args;
    BracesBlock code;
};

}

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::LabelIdent,
    (std::string, value)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::DefIdent,
    (std::string, value)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::SlotIdent,
    (std::string, value)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::TypeExpr,
    (std::string, raw)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::TermExpr,
    (std::string, raw)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::TupleExpr,
    (std::vector<Syntax::Expr>, elements)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::BracesBlock,
    (std::vector<Syntax::Expr>, stmts)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::LabelAssignment,
    (Syntax::Expr, value)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::LabelExpr,
    (boost::optional<Syntax::LabelIdent>, name)
    (boost::optional<Syntax::TypeExpr>, type)
    (boost::optional<Syntax::LabelAssignment>, term)
)

BOOST_FUSION_ADAPT_STRUCT(
    Syntax::DefExpr,
    (boost::optional<Syntax::DefIdent>, name)
    (boost::optional<Syntax::TupleExpr>, args)
    (Syntax::BracesBlock, code)
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
        ident_str = qi::lexeme[qi::alpha >> *(qi::alnum | qi::char_('_') )];

        def_ident = ident_str;
        slot_ident = ident_str;

        label_ident = -ident_str >> qi::lit(":");

        label_assignment = qi::lit("=") > term_expr;

        label_expr = label_ident >> -type_expr >> -label_assignment;

        expr_list = expr % ",";
        tuple_expr = expr_list;

        paren_arg_list %= qi::lit("(") > -tuple_expr > qi::lit(")");

        braces_block = qi::lit("{") > stmt_list > qi::lit("}");

        def_expr = qi::lit("def") > -def_ident > -paren_arg_list > braces_block;

        fun_call = ident_str >> qi::omit[-paren_arg_list] >> qi::omit[-braces_block];

        number = qi::lexeme[+qi::digit];
        quoted_string = qi::lexeme[qi::lit('"') > *(qi::char_-'"') > '"'];

        paren_expr %= qi::lit('(') > expr > ')';

        term_expr = dummy_str;

        dummy_str = fun_call | number | quoted_string;

        type_expr = dummy_str;

        expr = def_expr | label_expr | type_expr | term_expr;

        stmt_list = expr % +qi::char_("\n;");
    }

    qi::rule<Iterator, std::string(), Skipper<Iterator>> dummy_str;

    qi::rule<Iterator, std::string(), Skipper<Iterator>> ident_str;
    qi::rule<Iterator, Syntax::DefIdent(), Skipper<Iterator>> def_ident;
    qi::rule<Iterator, Syntax::SlotIdent(), Skipper<Iterator>> slot_ident;

    // Labels
    qi::rule<Iterator, Syntax::LabelIdent(), Skipper<Iterator>> label_ident;
    qi::rule<Iterator, Syntax::LabelAssignment(), Skipper<Iterator>> label_assignment;
    qi::rule<Iterator, Syntax::LabelExpr(), Skipper<Iterator>> label_expr;

    qi::rule<Iterator, Syntax::Expr(), Skipper<Iterator>> expr;
    qi::rule<Iterator, Syntax::TupleExpr(), Skipper<Iterator>> tuple_expr;
    qi::rule<Iterator, Syntax::DefExpr(), Skipper<Iterator>> def_expr;
    qi::rule<Iterator, Syntax::TypeExpr(), Skipper<Iterator>> type_expr;
    qi::rule<Iterator, Syntax::TermExpr(), Skipper<Iterator>> term_expr;

    qi::rule<Iterator, Syntax::TupleExpr(), Skipper<Iterator>> paren_arg_list;
    qi::rule<Iterator, Syntax::BracesBlock(), Skipper<Iterator>> braces_block;

    qi::rule<Iterator, std::string(), Skipper<Iterator>> number;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> quoted_string;
    qi::rule<Iterator, std::vector<Syntax::Expr>(), Skipper<Iterator>> stmt_list;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> fun_call;
    qi::rule<Iterator, Syntax::Expr(), Skipper<Iterator>> paren_expr;

    qi::rule<Iterator, std::vector<Syntax::Expr>(), Skipper<Iterator>> expr_list;
};


Syntax::Expr Parse(std::string str)
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
        //throw;
    }
}


int main(int argc, char* argv[])
{
    auto result = Parse("def Main(x:, y:) { a:Int = 3 }");
    //cerr << "Got: " << result << endl;
    return 0;
}
