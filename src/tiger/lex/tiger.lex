%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */
 /* TODO: Put your lab2 code here */
digit   [0-9]
letter [a-zA-Z]
digits	[0-9]+
character [0-9A-Za-z_]
quote \"
invisible [\000-\040]

%x COMMENT STR IGNORE

%%

 /* reserved words */
"array" {adjust(); return Parser::ARRAY;}
 /* TODO: Put your lab2 code here */
"," {adjust(); return Parser::COMMA;}
":" {adjust(); return Parser::COLON;}
";" {adjust(); return Parser::SEMICOLON;}
"(" {adjust(); return Parser::LPAREN;}
")" {adjust(); return Parser::RPAREN;}
"[" {adjust(); return Parser::LBRACK;}
"]" {adjust(); return Parser::RBRACK;}
"{" {adjust(); return Parser::LBRACE;}
"}" {adjust(); return Parser::RBRACE;}
"." {adjust(); return Parser::DOT;}
"+" {adjust(); return Parser::PLUS;}
"-" {adjust(); return Parser::MINUS;}
"*" {adjust(); return Parser::TIMES;}
"/" {adjust(); return Parser::DIVIDE;}
"=" {adjust(); return Parser::EQ;}
"<>" {adjust(); return Parser::NEQ;}
"<" {adjust(); return Parser::LT;}
"<=" {adjust(); return Parser::LE;}
">" {adjust(); return Parser::GT;}
">=" {adjust(); return Parser::GE;}
"&" {adjust(); return Parser::AND;}
"|" {adjust(); return Parser::OR;}
":=" {adjust(); return Parser::ASSIGN;}
"if" {adjust(); return Parser::IF;}
"then" {adjust(); return Parser::THEN;}
"else" {adjust(); return Parser::ELSE;}
"while" {adjust(); return Parser::WHILE;}
"for" {adjust(); return Parser::FOR;}
"to" {adjust(); return Parser::TO;}
"do" {adjust(); return Parser::DO;}
"let" {adjust(); return Parser::LET;}
"in" {adjust(); return Parser::IN;}
"end" {adjust(); return Parser::END;}
"of" {adjust(); return Parser::OF;}
"break" {adjust(); return Parser::BREAK;}
"nil" {adjust(); return Parser::NIL;}
"function" {adjust(); return Parser::FUNCTION;}
"var" {adjust(); return Parser::VAR;}
"type" {adjust(); return Parser::TYPE;}

 /*
  * ID
  */

{letter}{character}* {adjust(); return Parser::ID;}

 /*
  * INT
  */

{digits} {adjust(); return Parser::INT;}

 /*
  * special
  */

{quote}   {adjust(); begin(StartCondition__::STR);}

<STR>     {
    "\\n"       {adjustStr(); string_buf_ += '\n';}
    "\\t"       {adjustStr(); string_buf_ += '\t';}

    "\\"{digit}{digit}{digit} {
        adjustStr();
        std::string str = matched();
        string_buf_ += char((str[3] - '0') + (str[2] - '0') * 10 + (str[1] - '0') * 100);
    }

    "\\\""      {adjustStr(); string_buf_ += '\"';}
    "\\\\"      {adjustStr(); string_buf_ += '\\';}

    {quote}   {
      adjustStr();
      setMatched(string_buf_);
      string_buf_ = "";
      begin(StartCondition__::INITIAL); 
      return Parser::STRING;
    }
    .           {adjustStr(); string_buf_ += matched();}

    "\\"{invisible}       {
        adjustStr(); 
        ignore_buf_ += matched();
        begin(StartCondition__::IGNORE);
    }

    "\\^"[A-Z]        {adjustStr(); string_buf_ += char(matched()[2] - 'A' + 1);}
}

<IGNORE>  {
    "\\"              {adjustStr(); ignore_buf_ = ""; begin(StartCondition__::STR);}
    {invisible}       {adjustStr(); ignore_buf_ += matched();}
    .                 {adjustStr(); ignore_buf_ += matched(); string_buf_ += ignore_buf_; ignore_buf_ = ""; begin(StartCondition__::STR);}
}

"/*"      {adjust(); comment_level_++; begin(StartCondition__::COMMENT);}

<COMMENT> {
    "/*" {adjustStr(); comment_level_++;}
    "*/" {
      adjustStr();
      comment_level_--;
      if (comment_level_ == 1) {
          begin(StartCondition__::INITIAL);
      }
    }
    \n {adjustStr();}
    .  {adjustStr();}
}


 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
