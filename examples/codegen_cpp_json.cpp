#include <ostream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include "..\supercomplex.hpp"

using namespace std;
using namespace supercomplex;

struct t_info
{
	std::string name;
	bool skip;
};

/* Represent character as a C++ character literal to make the output a bit more readable */
std::string represent_char(char ch)
{
	std::stringstream stream;
	stream << "'";

	if (ch == '\r') stream << "\\r";
	else if (ch == '\n') stream <<  "\\n";
	else if (ch == '\t') stream << "\\t";
	else if (ch == '\\') stream << "\\\\";
	else if (ch == '\'') stream << "\\'";
	else if ((ch >= ' ') && (ch < '\x7f')) stream << ch;
	else 
	{
		stream << "\\x" << std::setfill('0') << std::setw(2) << std::hex << (static_cast<int>(ch) & 0xFF);
	}

	stream << "'";
	return stream.str();
}

/* Represent interval set as a bool condition that can be used in IF clause */
void ranges(std::basic_ostream<char>& out, const std::basic_string<char>& name, boost::icl::interval_set<char> range)
{
	if (boost::icl::interval_count(range) > 1)
		out << "(";
	bool first = true;
	for (auto&& interval : range)
	{
		if (!first)
		{
			out << " || ";
		}
		if (interval.lower() == interval.upper())
			out << "(" << name << " == " << represent_char(interval.upper()) << ")";
		else
			out << "(" << name << " >= " << represent_char(interval.lower()) << " && "
			    << name << " <= " << represent_char(interval.upper()) << ")";
		first = false;
	}
	if (boost::icl::interval_count(range) > 1)
		out << ")";
}


int cpp_codegen(std::basic_ostream<char>& out, const supercomplex::lexer<char, t_info>& automaton)
{
	out << "#include <fstream>" << std::endl;
	out << "#include <iostream>" << std::endl;
	out << "#include <sstream>" << std::endl;
	out << "#include <exception>" << std::endl;
	out << "#include <stdexcept>" << std::endl;
	
	std::string token_type_class = "token_type";
	std::string token_class = "token";
	std::string iterator_class = "lexer_iterator";

	out << "enum class " << token_type_class << " : int {\n    ENDOFFILE = -1," << std::endl;
	std::unordered_set<std::string> visited_terminals;
	std::unordered_set<std::string> visited_terminals2;

	/* We find all terminal nodes with names to populate the enum for token types. */
	int index = 0;
	for (auto&& state : automaton.states())
	{
		if (state.terminal && !state.terminal_info.skip && visited_terminals.find(state.terminal_info.name) == visited_terminals.end()) 
		{
			out << "    " << state.terminal_info.name << " = " << (index++) << "," << std::endl;
			visited_terminals.insert(state.terminal_info.name);
		}	
	}

	out << "};" << std::endl << std::endl;

	out << "const char* token_names[] = {\n    \"ENDOFFILE\"," << std::endl;
	index = 0;
	for (auto&& state : automaton.states())
	{
		if (state.terminal && !state.terminal_info.skip && visited_terminals2.find(state.terminal_info.name) == visited_terminals2.end())
		{
			out << "    \"" << state.terminal_info.name << "\"," << std::endl;
			visited_terminals2.insert(state.terminal_info.name);
		}
	}

	out << "};" << std::endl << std::endl;


	out << "struct " << token_class << " {" << std::endl;
	out << "    "<< token_type_class <<" type;" << std::endl;
	out << "    std::string value;" << std::endl;
	out << "};" << std::endl << std::endl;

	out << "template<typename input_iterator_t>" << std::endl;
	out << "struct " << iterator_class << " " << std::endl;
	out << "{" << std::endl;
	out << "public:" << std::endl;
	out << "    typedef lexer_iterator<input_iterator_t> self_type;" << std::endl;
	out << "    typedef " << token_class << " value_type;" << std::endl;
	out << "    typedef " << token_class << "& reference;" << std::endl;
	out << "    typedef " << token_class << "* pointer;" << std::endl;
	out << "    typedef std::forward_iterator_tag iterator_category;" << std::endl << std::endl;

	out << "    " << iterator_class << "(input_iterator_t begin, input_iterator_t end) : state_(" << automaton.start() << "), position_(begin), end_(end) { next(); };" << std::endl;
	out << "    " << iterator_class << "() : state_(-1) {};" << std::endl;
	out << "    const reference operator*() { return value_; }" << std::endl;
	out << "    const pointer operator->() { return &value_; } " << std::endl;
	out << "    bool operator==(const self_type& rhs) { return state_ == rhs.state_ && position_ == rhs.position_ && end_ == rhs.end_; }" << std::endl;
	out << "    bool operator!=(const self_type& rhs) { return state_ != rhs.state_ || position_ != rhs.position_ || end_ != rhs.end_; }" << std::endl;

	out << "    self_type operator++() { self_type i = *this; next(); return i; }" << std::endl;
	out << "    self_type operator++(int junk) { next(); return *this; }" << std::endl << std::endl;

	out << "    void next()" << std::endl;
	out << "    {" << std::endl;
	out << "        std::stringstream buffer;" << std::endl;

	out << "        for (;;) " << std::endl;
	out << "        {" << std::endl;
	out << "            if (position_ == end_ && state_ == " << automaton.start() << ") break;" << std::endl;

	out << "            switch (state_) {" << std::endl;

	const auto& states = automaton.states();

	for (size_t i = 0; i < states.size(); ++i)
	{
		const auto& state = states[i];

		out << "                case " << i << ":" << std::endl;
		bool first = true;
		for (auto&& transition : state.transitions)
		{
			if (first)
				out << "                    if ((position_ != end_) && ";
			else
				out << "                    else if ((position_ != end_) && ";
			ranges(out, "*position_", transition.characters);
			out << ")" << std::endl
			    << "                        state_ = " << transition.next << ";" << std::endl;
			first = false;
		}
		if (!first)
			out << "                    else {" << std::endl;
		if (state.terminal)
		{
			auto terminal_node = state.terminal_info;

			out << "                        state_ = " << automaton.start() << ";" << std::endl;
			if (!terminal_node.skip)
			{
				out << "                        value_ = value_type { " << token_type_class << "::" << terminal_node.name << ", buffer.str() };" << std::endl;
				out << "                        return;" << std::endl;
			}
			else
			{
				out << "                        buffer = std::stringstream();" << std::endl;
				out << "                        continue;" << std::endl;
			}
		}
		else
		{
			out << "                        throw std::runtime_error(\"Invalid input\");" << std::endl;
		}
		if (!first)
			out << "                    }" << std::endl;

		out << "                  break;" << std::endl;
	}
	
	out << "            }" << std::endl;
	out << "            if (position_ != end_)" << std::endl;
	out << "                buffer << *position_++;" << std::endl;
	out << "            else" << std::endl;
	out << "                break;" << std::endl;
	out << "        }" << std::endl;

	out << "        state_ = -1;\nvalue_ = value_type{ token_type::ENDOFFILE, "" };" << std::endl;
	out << "    }" << std::endl;
	
	out << "private:" << std::endl;
	out << "    value_type value_;" << std::endl;
	out << "    input_iterator_t position_;" << std::endl;
	out << "    input_iterator_t end_;" << std::endl;
	out << "    int state_;" << std::endl;	
	out << "};" << std::endl << std::endl;
	
	/*
	out << "int main()" << std::endl;
	out << "{" << std::endl;
	out << "    for (auto lex = " << iterator_class << "<std::istreambuf_iterator<char>>(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()); lex != " << iterator_class << "<std::istreambuf_iterator<char>>(); ++lex)" << std::endl;
	out << "    {" << std::endl;
	out << "       std::cout << (int)lex->type << \",\" << lex->value << std::endl;" << std::endl;
	out << "    }" << std::endl;
	out << "}" << std::endl;
	*/
	
	return 0;
}

lexer_production<char, t_info> prod(const std::string& name,
    const std::string& regex)
{
	return { { name, false }, regex };
}

lexer_production<char, t_info> skip(const std::string& regex)
{
	return { { std::string(), true }, regex };
}

int main()
{
	/*
		This will generate C++ code for a JSON lexer.
	*/
	lexer_generator<char, t_info> lex_gen;
	lex_gen 
		<< prod("OPEN_BRACKET", "\\[")
		<< prod("CLOSE_BRACKET", "\\]")
		<< prod("OPEN_PAREN", "\\(")
		<< prod("CLOSE_PAREN", "\\)")
		<< prod("OPEN_BRACKET", "{")
		<< prod("CLOSE_BRACKET", "}")	
		<< prod("PERIOD", ".")
		<< prod("AMP", "&")
		<< prod("PIPE", "\\|")
		<< prod("AND", "&&")
		<< prod("OR", "\\|\\|")
		<< prod("PLUS", "\\+")
		<< prod("MINUS", "\\-")
		<< prod("TILDE", "~")
		<< prod("MUL", "\\*")
		<< prod("DIV", "/")
		<< prod("BANG", "!")
		<< prod("MOD", "%")
		<< prod("ASSIGN", "=")
		<< prod("ADD_ASSIGN", "\\+=")
		<< prod("SUB_ASSIGN", "\\-=")
		<< prod("MUL_ASSIGN", "\\*=")
		<< prod("DIV_ASSIGN", "/=")
		<< prod("AND_ASSIGN", "&=")
		<< prod("OR_ASSIGN", "\\|=")
		<< prod("XOR_ASSIGN", "^=")
		<< prod("MOD_ASSIGN", "%=")
		<< prod("SHL_ASSIGN", "<<=")
		<< prod("SHR_ASSIGN", ">>=")
		<< prod("XOR", "^")
		<< prod("QMARK", "\\?")
		<< prod("SHL", "<<")
		<< prod("SHR", ">>")
		<< prod("LT", "<")
		<< prod("GT", ">")
		<< prod("LE", "<=")
		<< prod("GE", ">=")
		<< prod("NE", "!=")
		<< prod("EQ", "==")
		<< prod("PREPROCESS_LINE", "#[^\n]*")
		<< prod("IDENT", "[_a-zA-Z][_0-9a-zA-Z]*")
		<< prod("LITERAL", "auto|double|int|struct|break|else|long|switch|case|enum|register|typedef|char|extern|return|union|const|float|short|unsigned|continue|for|signed|void|default|goto|sizeof|volatile|do|if|static|while|_Bool|_Imaginary|restrict|_Complex|inline|_Alignas|_Generic|_Thread_local|_Alignof|_Noreturn|_Atomic|_Static_assert")
		<< prod("COMMA", ",")
		<< prod("COLON", ":")				
		<< prod("SEMICOLON", ";")
		<< prod("STRING", "(L|u|U|u8)?R?\"(\\\\([\"'\\\\/bfrntav0]|[0-7][0-7][0-7]|x[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]|u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])|[^\"\\\\\0-\x1f])*\"s?"s)
		<< prod("CHAR", "(L|u|U|u8)?R?'(\\\\([\"'\\\\/bfrntav0]|[0-7][0-7][0-7]|x[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]|u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])|[^\'\\\\\0-\x1f])*'"s)
		<< prod("NUMBER", "-?(0|[1-9][0-9]*)(.[0-9]+)?([Ee][+\\-]?(0|[1-9][0-9]*))?(u|U)?(d|f|LL|L)?")
//		<< skip("/\\*[^\\*]*\\*/")
		<< skip("/\\*[^\\*]*(\\*[^/][^\\*]*)*\\*/")
		<< skip("//[^\n\r]*[\r\n]")
		<< skip("[ \t\n\r]+")
		;

	auto lexer = lex_gen.generate();
	cpp_codegen(std::cout, lexer);
}
