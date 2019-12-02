%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */

%x COMMENT STR

%%

 /*
  *
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */
 /*
  * skip white space chars.
  * space, tabs and LF
  */


[ \t]+          {adjust();}
\n              {adjust(); errormsg.Newline();}
array           {adjust(); return Parser::ARRAY;}
type            {adjust(); return Parser::TYPE;}
let             {adjust(); return Parser::LET;}
var             {adjust(); return Parser::VAR;}
function        {adjust(); return Parser::FUNCTION;}
nil             {adjust(); return Parser::NIL;}
break           {adjust(); return Parser::BREAK;}
of              {adjust(); return Parser::OF;}
end             {adjust(); return Parser::END;}
in              {adjust(); return Parser::IN;}
do              {adjust(); return Parser::DO;}
to              {adjust(); return Parser::TO;}
for             {adjust(); return Parser::FOR;}
while           {adjust(); return Parser::WHILE;}
else            {adjust(); return Parser::ELSE;}
then            {adjust(); return Parser::THEN;}
if              {adjust(); return Parser::IF;}
:=              {adjust(); return Parser::ASSIGN;}
\|              {adjust(); return Parser::OR;}
&               {adjust(); return Parser::AND;}
\<\>            {adjust(); return Parser::NEQ;}
\>=             {adjust(); return Parser::GE;}
\>              {adjust(); return Parser::GT;}
\<=             {adjust(); return Parser::LE;}
\<              {adjust(); return Parser::LT;}
=               {adjust(); return Parser::EQ;}
\/              {adjust(); return Parser::DIVIDE;}
\*              {adjust(); return Parser::TIMES;}
\-              {adjust(); return Parser::MINUS;}
\+              {adjust(); return Parser::PLUS;}
\.              {adjust(); return Parser::DOT;}
\{              {adjust(); return Parser::LBRACE;}
\}              {adjust(); return Parser::RBRACE;}
\(              {adjust(); return Parser::LPAREN;}
\)              {adjust(); return Parser::RPAREN;}
\[              {adjust(); return Parser::LBRACK;}
\]              {adjust(); return Parser::RBRACK;}
:               {adjust(); return Parser::COLON;}
;               {adjust(); return Parser::SEMICOLON;}
,               {adjust(); return Parser::COMMA;}
[a-zA-Z_]+[0-9a-zA-Z_]*       {
                                adjust(); 
                                return Parser::ID;
                              }
[0-9]+          {adjust(); return Parser::INT;}
/*string change condition with buffer storing translated escape sequences*/
\"              {
                    adjust();
                    stringBuf_.clear();
                    begin(StartCondition__::STR);
                }
/*unclosed problem*/
<STR><<EOF>> {errormsg.Error(errormsg.tokPos, "unclosed string");}
<STR>{
    \"          {
                    adjustStr();
                    setMatched(stringBuf_);
                    begin(StartCondition__::INITIAL);
                    return Parser::STRING;
                }
    [^"\\]+       { adjustStr();stringBuf_ += matched();} 
    \\[ \n\t]+\\       { adjustStr();}
    \\n           { adjustStr();stringBuf_ += '\n';} 
    \\t           { adjustStr();stringBuf_ += '\t';}
    \\\^[A-Z@]         { adjustStr();
                                char ch;
                                switch (matched().at(2)) {
                                  case '@': ch = '\0';
                                  case '[': ch = '\e';break;
                                  default: ch = matched().at(2) - 64;
                                }
                                stringBuf_ += ch;
                              } 
    \\            { adjustStr();stringBuf_ += '\\';}
    \\[0-9]{3}    { adjustStr();stringBuf_ += atoi(matched().substr(1).c_str());}
    \\.           { adjustStr();stringBuf_ += matched().at(1);}
    \\f___f\\     { adjustStr();}
}

/*comment start, build stack*/
\/\*            {
                    adjust();
                    commentLevel_++;
                    begin(StartCondition__::COMMENT);
                }
<COMMENT><<EOF>>            {errormsg.Error(errormsg.tokPos, "unclosed string");}
<COMMENT>{
    \/\*        {
                  adjustStr();
                  commentLevel_++;
                }
    \*\/        {
                  adjustStr();
                  if (--commentLevel_ == 0) {
                    begin(StartCondition__::INITIAL); 
                  }
                }     
    .|\n           {adjustStr();}
}

.               {adjust(); errormsg.Error(errormsg.tokPos, "illegal token");}
