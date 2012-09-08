
#include <iostream>
#include <string>

using namespace std;

#include "boost/spirit/include/qi.hpp"
#include "boost/fusion/include/io.hpp"
#include "boost/spirit/include/karma.hpp"

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

/*
namespace Lang {

struct LabelExpr
{
    boost::optional<LabelIdent> name;
    boost::optional<TypeExpr> type;
    boost::optional<TermExpr> term;
};

struct DefExpr
{
    boost::optional<DefIdent> name;
    boost::optional<Tuple> args;
    Block code;
};

}
*/

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
struct LangParseGrammar : qi::grammar<Iterator, std::string(), Skipper<Iterator>>
{
    LangParseGrammar() : LangParseGrammar::base_type(start)
    {
        start = defExpr;

        ident = qi::lexeme[qi::alpha >> *(qi::alnum | qi::char_('_') )];

        labelExpr = -ident >> ":" >> -expr >> -(qi::lit("=") > expr);

        tuple = expr % ",";
        paren_arg_list = qi::lit("(") >> tuple >> qi::lit(")");

        block = qi::lit("{") > stmt_list > qi::lit("}");
        defExpr = qi::lit("def") >> -ident >> -paren_arg_list > block;
        fun_call = ident >> -paren_arg_list >> -block;

        number = qi::lexeme[+qi::digit];
        quoted_string = qi::lexeme[qi::lit('"') > *(qi::char_-'"') > '"'];
        parenExpr = qi::lit('(') > expr > ')';

        expr = (defExpr | labelExpr | fun_call | number | quoted_string | block);

        stmt_list = expr % +qi::char_("\n;");
    }

    qi::rule<Iterator, std::string(), Skipper<Iterator>> start;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> defExpr;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> ident;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> labelExpr;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> paren_arg_list;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> expr;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> tuple;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> block;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> number;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> quoted_string;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> stmt_list;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> fun_call;
    qi::rule<Iterator, std::string(), Skipper<Iterator>> parenExpr;
};


string Parse(std::string str)
{
    typedef std::string::const_iterator iterator_type;
    typedef LangParseGrammar<iterator_type> LangGrammar;

    LangGrammar g; // Our grammar

    std::string::const_iterator iter = str.begin();
    std::string::const_iterator end = str.end();
    Skipper<iterator_type> skipper;
    string result;
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


int main(int argc, char* argv[])
{
    Parse("def Main() { a: = 1 }");
}
