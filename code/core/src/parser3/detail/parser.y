%skeleton "lalr1.cc" /* -*- C++ -*- */
%require "3.0.4"
%defines
%define parser_class_name {inspire_parser}
%define api.namespace {insieme::core::parser3::detail}
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%code requires
{
    /**
     * this code goes in the header
     */

    # include <string>
    #include "insieme/core/ir.h"

    namespace insieme{
    namespace core{
    namespace parser3{
    namespace detail{

        class inspire_driver;
        class inspire_scanner;

    } // detail
    } // parser3
    } // core
    } // insieme

    using namespace insieme::core;

    namespace {
        struct For_decl{
            VariablePtr it;
            ExpressionPtr low;
            ExpressionPtr up;
            ExpressionPtr step;
        };
    }
}

// The parsing context.
%param { parser3::detail::inspire_driver& driver }
%param { parser3::detail::inspire_scanner& scanner }
%locations
%initial-action
{
  // Initialize the initial location.
  @$.initialize(&driver.file);
};

// DEBUG
//%define parse.trace
%define parse.error verbose
%code
{
    /**
     * this code goes in the cpp file
     */

    #include "insieme/core/parser3/detail/driver.h"
    #include "insieme/core/parser3/detail/scanner.h"
    #include "insieme/core/ir.h"

    #include "insieme/core/annotations/naming.h"
    #include "insieme/core/analysis/ir_utils.h"

    #define INSPIRE_MSG(l, n, msg) \
            if(!n) { driver.error(l, msg); YYABORT; }
    #define INSPIRE_TYPE(l, n, t, msg) \
            if(!n->getType().isa<t>()) { driver.error(l, msg); YYABORT; }
    #define INSPIRE_GUARD(l, n) \
            if(!n) { driver.error(l, "unrecoverable error"); YYABORT; }
    #define INSPIRE_NOT_IMPLEMENTED(l) \
            { driver.error(l, "not supported yet "); YYABORT; }
    #define RULE if(driver.inhibit_building()) { break; }
}

%define api.token.prefix {TOK_}
%token 
  END  0  "end of stream"
  ASSIGN  "="
  MINUS   "-"
  PLUS    "+"
  STAR    "*"
  SLASH   "/"

  LPAREN  "("
  RPAREN  ")"
  LCURBRACKET  "{"
  RCURBRACKET  "}"
  LBRACKET  "["
  RBRACKET  "]"
  DQUOTE  "\""
  QUOTE  "\'"

  LT        "<"     
  GT        ">"     
  LEQ       "<="    
  GEQ       ">="    
  EQ        "=="    
  NEQ       "!="    

  BAND      "&"
  BOR       "|"
  BXOR      "^"

  LAND      "&&"
  LOR       "||"
  LNOT      "!"
                                         
  QMARK   "?"
  COLON   ":"
  NAMESPACE "::"
  FUNNY_BOY "~"

  ARROW   "->"
  DARROW  "=>"

  SEMIC   ";"
  COMA    ","
  RANGE   ".."
  DOT     "."
  ADDRESS "$"
  
  PARENT        ".as("
  CAST          "CAST("
  INFINITE      "#inf"
  LET           "let"    
  AUTO          "auto"    
  LAMBDA        "lambda"    
  CTOR          "ctor"    

  IF            "if"      
  ELSE          "else"    
  FOR           "for"     
  WHILE         "while"   
  DECL          "decl"
  TRY           "try"   
  CATCH         "catch"   
  RETURN        "return"  
  DEFAULT       "default"  
  CASE          "case"  
  SWITCH        "switch"  
  
  VAR           "var"
  NEW           "new"
  LOC           "loc"
  DELETE        "delete"
  UNDEFINED     "undefined"

  TYPE_LITERAL  "type("
  LITERAL       "lit("
  PARAM         "param("

  TRUE          "true"  
  FALSE         "false"  
  STRUCT        "struct"
  UNION         "union"

  SPAWN         "spawn"
  SYNC          "sync"
  SYNCALL       "syncAll"

  TYPE_ONLY
  EXPRESSION_ONLY
  STMT_ONLY
  FULL_PROGRAM
;

    /* operators: they use string for convenience, instead of creating million rules */
%token <std::string> BIN_OP 


    /* Literals */
%token <std::string> STRING "stringlit"
%token <std::string> CHAR "charlit"
%token <std::string> IDENTIFIER "identifier"
%token <std::string> TYPE_VAR   "type_var"
%token <std::string> BOOL "bool"
%token <std::string> INT "int"
%token <std::string> UINT "uint"
%token <std::string> LONG "long"
%token <std::string> ULONG "ulong"
%token <std::string> FLOAT "float"
%token <std::string> DOUBLE "double"
%token <std::string> PARAMVAR "paramvar"

%type  <std::string> "Number" 
%type  <std::string> "indentifier" 

%type <NodePtr> start_rule
%type <ProgramPtr> program

%type <StatementPtr> statement decl_stmt compound_stmt
%type <StatementList>  statement_list
%type <For_decl> for_decl
%type <VariablePtr> var_decl
%type <VariableList> variable_list  variable_list_aux
%type <SwitchCasePtr> switch_case
%type <std::vector<SwitchCasePtr> > switch_case_list
%type <std::vector<CatchClausePtr> > catch_clause_list

%type <ExpressionPtr> expression  lambda_expression markable_expression lambda_expression_aux
%type <ExpressionList> expression_list

%type <std::pair<TypeList, IntParamList> > type_param_list gen_type
%type <TypePtr> type tuple_or_function  struct_type named_type
%type <TypeList> type_list parent_list type_list_aux
%type <FunctionKind> func_tok

%type <NamedTypeList> member_list tag_def union_type 

%type <NamedValuePtr> assign_field 
%type <NamedValueList> assign_rules 


%printer { yyoutput << $$; } <std::string>



/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
%%

/* ~~~~~~~~~~~~ Super rule, the lexer is prepared to return a spetial token and jump to the right rule ~~~~~~~~~~~~ */

%start start_rule;
start_rule : TYPE_ONLY declarations type             { RULE if(!driver.where_errors())  driver.result = $3; }
           | STMT_ONLY statement                     { RULE if(!driver.where_errors())  driver.result = $2; }
           | EXPRESSION_ONLY declarations expression { RULE if(!driver.where_errors())  driver.result = $3; }
           | FULL_PROGRAM  declarations { RULE driver.open_scope(@$, "program"); }  program { RULE if(!driver.where_errors()) driver.result = $4; }
           ;

/* ~~~~~~~~~~~~~~~~~~~  LET ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

let_defs : "lambda" lambda_expression ";" 
         | "lambda" lambda_expression "," let_defs 
         | type ";" { RULE driver.add_let_type(@1, $1); }
         | type "," { RULE driver.add_let_type(@1, $1); } let_defs                       
         ;

let_decl : "identifier" { if(driver.let_count==1) driver.add_let_name(@1,$1); } "=" let_defs  { }
         | "identifier" { if(driver.let_count==1) driver.add_let_name(@1,$1); } "," let_decl  { }
         ;

let_chain : let_decl { driver.close_let_statement(@$); } 
          | let_decl { driver.close_let_statement(@$); } "let" { driver.let_count++; } let_chain { }
          ;

declarations : /* empty */ { } 
             | "let" { driver.let_count++; } let_chain { }
             ;

/* ~~~~~~~~~~~~~~~~~~~  PROGRAM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

program : type "identifier" "(" variable_list "{" compound_stmt { RULE
                             INSPIRE_GUARD(@1, $1); 
                             driver.close_scope(@$, "program");
                             TypeList paramTys;
                             for (const auto& var : $4) paramTys.push_back(var.getType());
                             FunctionTypePtr funcType = driver.builder.functionType(paramTys, $1); 
						     ExpressionPtr main = driver.builder.lambdaExpr(funcType, $4, $6);
						     $$ = driver.builder.createProgram(toVector(main));
                        }
        ;


variable_list : ")" { }
              | var_decl ")" { RULE $$.push_back($1); }
              | var_decl "," variable_list_aux ")" {RULE  $3.insert($3.begin(), $1); std::swap($$, $3); } 
              ;

variable_list_aux : var_decl {  RULE $$.push_back($1); }
                  | var_decl "," variable_list_aux { RULE $3.insert($3.begin(), $1); std::swap($$, $3); }
                  ;
/* ~~~~~~~~~~~~~~~~~~~  TYPES ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

type_param_list : "#inf"     { RULE $$.second.insert($$.second.begin(), driver.builder.infiniteIntTypeParam()); }
                | "paramvar" { RULE $$.second.insert($$.second.begin(), driver.gen_type_param_var(@1,$1)); }
                | "int"      { RULE $$.second.insert($$.second.begin(), 
                                    driver.builder.concreteIntTypeParam(utils::numeric_cast<uint32_t>($1))); }
                | type       { RULE $$.first.insert($$.first.begin(), $1); }

                | "#inf" "," type_param_list { RULE
                        $3.second.insert($3.second.begin(), driver.builder.infiniteIntTypeParam()); 
                        std::swap($$.first, $3.first);
                        std::swap($$.second, $3.second);
                    }
                | "paramvar" "," type_param_list {RULE
                        $3.second.insert($3.second.begin(), driver.gen_type_param_var(@1,$1)); 
                        std::swap($$.first, $3.first);
                        std::swap($$.second, $3.second);
                    }
                | "int" "," type_param_list { RULE
                        $3.second.insert($3.second.begin(), driver.builder.variableIntTypeParam($1[1])); 
                        std::swap($$.first, $3.first);
                        std::swap($$.second, $3.second);
                    }
                | type "," type_param_list { RULE
                        $3.first.insert($3.first.begin(), $1); 
                        std::swap($$.first, $3.first);
                        std::swap($$.second, $3.second);
                    }
                ;

type_list_aux : type ")"           { RULE $$.insert($$.begin(), $1); }
              | type "," type_list_aux { RULE $3.insert($3.begin(), $1); std::swap($$, $3); }

type_list : ")" { }
          | type_list_aux { RULE std::swap($$, $1); }

parent_list : type                 { RULE $$.insert($$.begin(), $1); }
            | type "," parent_list { RULE $3.insert($3.begin(), $1); std::swap($$, $3); }
            ;

func_tok : "->" { RULE $$ = FK_PLAIN; }
         | "=>" { RULE $$ = FK_CLOSURE; }
         ;

member_list : "}" {}
            | type "identifier" "}" { RULE $$.insert($$.begin(), driver.builder.namedType($2, $1)); }
            | type "identifier" ";" member_list { RULE $4.insert($4.begin(), driver.builder.namedType($2, $1)); std::swap($$, $4); }
            ;

tag_def : "{" member_list   { std::swap($$, $2); }
         ;

                     /* tuple */
tuple_or_function : "(" type_list  { RULE 
                            if($2.size() <2) error(@2, "tuple type must have 2 fields or more, sorry");
                            $$ = driver.builder.tupleType($2); 
                        }
                     /* function/closure type */
                  | "(" type_list func_tok type { RULE
                            $$ = driver.genFuncType(@$, $2, $4, $3); 
                            INSPIRE_GUARD(@$, $$);
                        }
                     /*  constructor */
                  | "ctor" type "::" "(" type_list { RULE
                            TypePtr classType = driver.builder.refType($2);
                            TypePtr retType = classType;
                            $5.insert($5.begin(), classType);
                            $$ = driver.builder.functionType($5, retType, FK_CONSTRUCTOR);
                        }
                     /* member function */
                  | type "::" "(" type_list "->" type { RULE
                            TypePtr classType = driver.builder.refType($1);
                            TypePtr retType = $6;
                            $4.insert($4.begin(), classType);
                            $$ = driver.builder.functionType($4, retType, FK_MEMBER_FUNCTION);
                        }
                     /*  destructor */
                  | "~" type "::" "(" ")" { RULE
						    TypePtr classType = driver.builder.refType($2);
                            $$ = driver.builder.functionType(toVector(classType), classType, FK_DESTRUCTOR);
                        }
                  ;


gen_type : type_param_list ">" { RULE std::swap($$.first, $1.first); std::swap($$.second, $1.second); }
         | ">"                 { }
         ;

union_type : tag_def   { RULE std::swap($$, $1); }
           ;

struct_type : tag_def              { RULE $$ = driver.builder.structType(driver.builder.stringValue(""), $1); }
            | "identifier" tag_def { RULE $$ = driver.builder.structType(driver.builder.stringValue($1), $2); }
            | "identifier" ":" parent_list tag_def { RULE 
                        $$ = driver.builder.structType(driver.builder.stringValue($1), driver.builder.parents($3), $4); 
                    }
            | ":" parent_list tag_def { RULE 
                        $$ = driver.builder.structType(driver.builder.parents($2), $3); 
                    }
            ;


type : "identifier" "<" gen_type { RULE $$ = driver.genGenericType(@$, $1, $3.first, $3.second); }
     | "identifier" { RULE         
                            $$ = driver.findType(@1, $1);
                            if(!$$) $$ = $$ = driver.genGenericType(@$, $1, TypeList(), IntParamList()); 
                            if(!$$) { driver.error(@$, format("undefined type %s", $1)); YYABORT; } 
                    }
     | "type_var"           { RULE $$ = driver.builder.typeVariable($1);  }
     | "struct" struct_type { RULE $$ = $2; }
     | "union"  union_type  { RULE $$ = driver.builder.unionType($2); }
     | tuple_or_function    { RULE $$ = $1; }
     ;

named_type : "identifier" "<" gen_type { RULE $$ = driver.genGenericType(@$, $1, $3.first, $3.second); }
           | "identifier" { RULE         
                                  $$ = driver.findType(@1, $1);
                                  if(!$$) $$ = $$ = driver.genGenericType(@$, $1, TypeList(), IntParamList()); 
                                  if(!$$) { driver.error(@$, format("undefined type %s", $1)); YYABORT; } 
                          }
           | "type_var"           { RULE $$ = driver.builder.typeVariable($1);  }
           ;

/* ~~~~~~~~~~~~~~~~~~~  STATEMENTS  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

statement : ";" { RULE $$ = driver.builder.getNoOp(); } 
          | expression ";" { RULE $$ = $1; }
          | "let" { driver.let_count++; } let_decl { driver.close_let_statement(@$); RULE $$ =  driver.builder.getNoOp(); }
                /* compound statement */ 
          | "{"  { RULE driver.open_scope(@$, "compound"); } compound_stmt  {RULE  
                                    driver.close_scope(@$, "compound_end"); $$ =$3; 
                 }

                /* variable declarations */
          | "decl" decl_stmt { RULE $$ = $2;  }

                /* if */
          | "if" "(" expression ")" statement {  RULE $$ = driver.builder.ifStmt($3, $5); }
          | "if" "(" expression ")" statement "else" statement { RULE $$ = driver.builder.ifStmt($3, $5, $7); }

                /* loops */
          | "while" "(" expression ")" statement { RULE $$ = driver.builder.whileStmt($3, $5); }
          | "for" { RULE driver.open_scope(@$, "forDecl"); } for_decl statement { RULE
                $$ = driver.builder.forStmt($3.it, $3.low, $3.up, $3.step, $4);
                driver.close_scope(@$, "for end");
            }
                /* switch */
          | "switch" "(" expression ")" "{" switch_case_list "}" { RULE
			    $$ = driver.builder.switchStmt($3, $6, driver.builder.getNoOp());
            }
          | "switch" "(" expression ")" "{" switch_case_list "default" ":" statement "}" { RULE
			    $$ = driver.builder.switchStmt($3, $6, $9);
            }
                /* exceptions */
          | "try" statement "catch" catch_clause_list { RULE
                if(!$2.isa<CompoundStmtPtr>()) { driver.error(@2, "try body must be a compound"); YYABORT; }
			    $$ = driver.builder.tryCatchStmt($2.as<CompoundStmtPtr>(), $4);
            }   
          | "return" expression ";" { RULE $$ = driver.builder.returnStmt($2); }
          | "return" ";" {  RULE $$ = driver.builder.returnStmt(driver.builder.getLangBasic().getUnitConstant()); }
          ;

catch_clause_list : 
                   "catch" "(" ")" statement { RULE
                            if(!$4.isa<CompoundStmtPtr>()) { driver.error(@4, "catch body must be a compound"); YYABORT; }
                            $$.push_back(driver.builder.catchClause(VariablePtr(), $4.as<CompoundStmtPtr>())); 
                    } 
                  | "(" var_decl ")" statement {  RULE
                            if(!$4.isa<CompoundStmtPtr>()) { driver.error(@4, "catch body must be a compound"); YYABORT; }
                            $$.push_back(driver.builder.catchClause($2, $4.as<CompoundStmtPtr>())); 
                    }
                  | "(" var_decl ")" statement "catch" catch_clause_list{ RULE
                            if(!$4.isa<CompoundStmtPtr>()) { driver.error(@4, "catch body must be a compound"); YYABORT; }
                            $6.insert($6.begin(), driver.builder.catchClause($2, $4.as<CompoundStmtPtr>()));
                            std::swap($$, $6);
                    }
                  ;

decl_stmt : var_decl ";" {RULE
                auto type = $1->getType();
                ExpressionPtr value = driver.builder.undefined(type);
                if (type.isa<RefTypePtr>()) {
                    value = driver.builder.refVar(driver.builder.undefined(type.as<RefTypePtr>()->getElementType()));
                }
                $$ = driver.builder.declarationStmt($1, value);
            }
          | var_decl "=" expression {RULE
                $$ = driver.builder.declarationStmt($1, $3);
            }
          | "auto" "identifier" "=" expression {RULE
		        auto var = driver.builder.variable($4.getType());
				annotations::attachName(var, $2);
                driver.add_symb(@$, $2, var);
                $$ = driver.builder.declarationStmt(var, $4);
            }
          ;

var_decl : type "identifier" { RULE
		        $$ = driver.builder.variable($1);
				annotations::attachName( $$, $2);
                driver.add_symb(@$, $2, $$);
            };


for_decl : "(" type "identifier" "=" expression ".." expression ")"  {RULE
		        $$.it = driver.builder.variable($2);
				annotations::attachName( $$.it, $3);
                driver.add_symb(@$, $3, $$.it);
                $$.low = $5;
                $$.up = $7;
                $$.step = driver.builder.literal($2, "1");
           }
         | "(" type "identifier" "=" expression ".." expression ":" expression ")" {RULE
		        $$.it = driver.builder.variable($2);
				annotations::attachName( $$.it, $3);
                driver.add_symb(@$, $3, $$.it);
                $$.low = $5;
                $$.up = $7;
                $$.step = $9;
           }
         ;

switch_case_list : switch_case { RULE $$.push_back($1); }
                 | switch_case switch_case_list { RULE $2.insert($2.begin(), $1); std::swap($$, $2); }
                 ;


switch_case :  "case" expression ":" statement { RULE
                  if(!$2.isa<LiteralPtr>()) { driver.error(@2, "case value must be a literal"); YYABORT; }
                  $$ = driver.builder.switchCase($2.as<LiteralPtr>(), $4); 
               }
            ;

compound_stmt : "}" { RULE $$ = driver.builder.compoundStmt(); }
              | statement_list "}"{ RULE $$ = driver.builder.compoundStmt($1); }
              ;

statement_list : statement { RULE $$.push_back($1); }
               | statement statement_list { RULE $2.insert($2.begin(),$1); std::swap($$, $2); }
               ;

/* ~~~~~~~~~~~~~~~~~~~  EXPRESSIONS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

expression : markable_expression         { RULE INSPIRE_GUARD(@1, $1); $$ = $1; } 
           | "$" markable_expression "$" {  RULE INSPIRE_GUARD(@2, $2); $$ = $2; } 

expression_list : expression                      { RULE $$.push_back($1); }
                | expression "," expression_list  { RULE $3.insert($3.begin(), $1); std::swap($$, $3); }
 
markable_expression : "identifier" { RULE $$ = driver.findSymbol(@$, $1); }
            /* unary */
           | "*" expression { RULE $$ = driver.builder.deref($2); } %prec UDEREF
           | "-" expression { RULE $$ = driver.builder.minus($2); } %prec UMINUS
           | expression "=" expression  { RULE $$ = driver.genBinaryExpression(@$, "=", $1, $3); }
            /* bitwise / logic / arithmetic / geometric */
           | expression "&" expression  { RULE $$ = driver.genBinaryExpression(@$, "&", $1, $3);  }
           | expression "|" expression  { RULE $$ = driver.genBinaryExpression(@$, "|", $1, $3);  }
           | expression "^" expression  { RULE $$ = driver.genBinaryExpression(@$, "^", $1, $3);  }
           | expression "&&" expression { RULE $$ = driver.genBinaryExpression(@$, "&&", $1, $3);  }
           | expression "||" expression { RULE $$ = driver.genBinaryExpression(@$, "||", $1, $3);  }
           | expression "+" expression  { RULE $$ = driver.genBinaryExpression(@$, "+", $1, $3);  }
           | expression "-" expression  { RULE $$ = driver.genBinaryExpression(@$, "-", $1, $3);  }
           | expression "*" expression  { RULE $$ = driver.genBinaryExpression(@$, "*", $1, $3);  }
           | expression "/" expression  { RULE $$ = driver.genBinaryExpression(@$, "/", $1, $3);  }
           | expression "==" expression { RULE $$ = driver.genBinaryExpression(@$, "==", $1, $3);  }
           | expression "!=" expression { RULE $$ = driver.genBinaryExpression(@$, "!=", $1, $3);  }
           | expression "<" expression  { RULE $$ = driver.genBinaryExpression(@$, "<", $1, $3);  }
           | expression ">" expression  { RULE $$ = driver.genBinaryExpression(@$, ">", $1, $3);  }
           | expression "<=" expression { RULE $$ = driver.genBinaryExpression(@$, "<=", $1, $3);  }
           | expression ">=" expression { RULE $$ = driver.genBinaryExpression(@$, ">=", $1, $3);  }
            /* data access */
           | expression "[" expression "]" { RULE $$ = driver.genBinaryExpression(@$, "[", $1, $3); }
           | expression "." "identifier" { RULE $$ = driver.genFieldAccess(@1, $1, $3); }
                
            /* ternary operator */
           | expression "?" expression ":" expression { RULE
						$$ =  driver.builder.ite($1, driver.builder.wrapLazy($3), driver.builder.wrapLazy($5));
                    }
            /* call expr */
           | expression "(" ")"                 { RULE INSPIRE_TYPE(@1, $1,FunctionTypePtr, "non callable symbol"); 
                                                  $$ = driver.genCall(@$, $1, ExpressionList());  }
           | expression "(" expression_list ")" { RULE INSPIRE_TYPE(@1, $1,FunctionTypePtr, "non callable symbol"); 
                                                  $$ = driver.genCall(@$, $1, $3); }
            /* parenthesis : tuple or expression */
           | "(" expression_list ")"  { RULE  
                                                  if ($2.size() == 1) $$ = $2[0];
                                                  else $$ = driver.builder.tupleExpr($2);
             }
            /* lambda or closure expression: callable expression */
           |  "lambda" lambda_expression  { RULE $$ = $2; }
            /* cast */ 
           | "CAST(" type ")" expression  { RULE $$ = driver.builder.castExpr($2, $4); }
           | expression ".as(" type ")"   { RULE $$  = driver.builder.refParent($1, $3); }
            /* ref mamagement */
           | "undefined" "(" type ")"     { RULE $$ =  driver.builder.undefined( $3 ); }
           | "var" "(" expression ")"     { RULE $$ =  driver.builder.refVar( $3 ); }
           | "new" "(" expression ")"     { RULE $$ =  driver.builder.refNew( $3 ); }
           | "loc" "(" expression ")"     { RULE $$ =  driver.builder.refLoc( $3 ); }
           | "delete" "(" expression ")"  { RULE $$ =  driver.builder.refDelete( $3 ); }
            /* literals */
           | "bool"       { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getBool(), $1); }
           | "charlit"    { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getChar(), $1); }
           | "int"        { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getInt4(), $1); }
           | "uint"       { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getUInt4(), $1); }
           | "long"       { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getInt8(), $1); }
           | "ulong"      { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getUInt8(), $1); }
           | "float"      { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getReal4(), $1); }
           | "double"     { RULE $$ = driver.builder.literal(driver.mgr.getLangBasic().getReal8(), $1); }
           | "stringlit"  { RULE $$ = driver.builder.stringLit($1); }
            /* constructed literals */
           | "type(" type ")"         { RULE $$ = driver.builder.getTypeLiteral($2); }
           | "param(" "paramvar" ")"  { RULE $$ = driver.builder.getIntTypeParamLiteral(driver.find_type_param_var(@2, $2)); }
           | "param(" "int" ")"       { RULE $$ = driver.builder.getIntTypeParamLiteral(driver.builder.concreteIntTypeParam(utils::numeric_cast<uint32_t>($2))); }
           | "lit(" "stringlit" ")"          { RULE $$ = driver.builder.getIdentifierLiteral($2); }
           | "lit(" "stringlit" ":" type ")" { RULE $$ = driver.builder.literal($4, $2); }
           | "lit(" type ")"                 { RULE $$ = driver.builder.getTypeLiteral($2); }
            /* struct / union expressions */
           | "struct" type "{" assign_rules { RULE $$ = driver.genTagExpression(@$, $2, $4); }
           | "struct" "{" assign_rules      { RULE $$ = driver.genTagExpression(@$, $3); }
            /* async */
           | "spawn" expression { RULE $$ = driver.builder.parallel($2);  }
           | "sync" expression  { RULE $$ = driver.builder.callExpr(driver.builder.getLangBasic().getUnit(), 
                                                                    driver.builder.getLangBasic().getMerge(), $2);
                                }
           | "syncAll" { RULE 
                    $$ = driver.builder.callExpr(driver.builder.getLangBasic().getUnit(), driver.builder.getLangBasic().getMergeAll()); 
                }
           ;

assign_field : "identifier" "=" expression { RULE $$ = driver.builder.namedValue($1, $3); }
             ;

assign_rules : "}" { }
             | assign_field "}" { RULE $$.insert($$.begin(), $1); }
             | assign_field "," assign_rules { RULE $3.insert($3.begin(), $1); std::swap($$, $3); }
             ;

lambda_expression_aux : 
                            /* closures */
                       "(" variable_list "=>" expression { RULE

                            if (driver.let_count == 1) {
                                driver.add_let_closure(@$, driver.genClosure(@1, $2, $4));
                            }
                            else {
                                $$ = driver.genClosure(@$, $2, $4);
                            }
                        } 
                      | "(" variable_list "=>" "{" compound_stmt  { RULE
                            
                            if (driver.let_count == 1) {
                                driver.add_let_closure(@$, driver.genClosure(@1, $2, $5));
                            }
                            else{
                                 $$ = driver.genClosure(@$, $2, $5);
                            }
                        } 
                            /* lambdas */
                      | "(" variable_list "->" type ":" { driver.set_inhibit(driver.let_count);} expression {     

                            if (driver.let_count ==1 ){
                                driver.add_let_lambda(@$, @1, @7, $4, $2);
                            }
                            else{
                                RULE $$ = driver.genLambda(@$, $2, $4, driver.builder.compoundStmt(driver.builder.returnStmt($7))); 
                            }
                            driver.set_inhibit(false);
                        } 
                      | "(" variable_list "->" type "{" { driver.set_inhibit(driver.let_count);} compound_stmt { 

                            if (driver.let_count ==1 ){
                                driver.add_let_lambda(@$, @1, @7, $4, $2);
                            }
                            else{
                                  RULE $$ = driver.genLambda(@$, $2, $4, $7); 
                            }
                            driver.set_inhibit(false);
                        } 
                            /* member functions */
                      | named_type "::" { driver.add_this(@1, $1); } "(" variable_list "->" type "{" 
                                        { driver.set_inhibit(driver.let_count);} compound_stmt { 

                            auto thisvar = driver.findSymbol(@1, "this");
                            $5.insert($5.begin(), thisvar.as<VariablePtr>());
                            if (driver.let_count ==1 ){
                                driver.add_let_lambda(@$, @1, @10, $7, $5, FK_MEMBER_FUNCTION);
                            }
                            else{
                                RULE $$ = driver.genLambda(@$, $5, $7, $10, FK_MEMBER_FUNCTION); 
                            }
                            driver.set_inhibit(false);
                        } 
                      | "~" named_type "::" { driver.add_this(@1, $2); } "(" variable_list "{" 
                                            { driver.set_inhibit(driver.let_count);} compound_stmt { 

                            auto thisvar = driver.findSymbol(@1, "this");
                            $6.insert($6.begin(),  thisvar.as<VariablePtr>());
                            if (driver.let_count ==1 ){
                                driver.add_let_lambda(@$, @1, @9, thisvar->getType(), $6, FK_DESTRUCTOR);
                            }
                            else{
                                RULE $$ = driver.genLambda(@$, $6, thisvar->getType(), $9, FK_DESTRUCTOR); 
                            }
                            driver.set_inhibit(false);
                        } 
                      | "ctor" named_type "::" { driver.add_this(@2, $2); } "(" variable_list "{" 
                                               { driver.set_inhibit(driver.let_count);} compound_stmt { 

                            auto thisvar = driver.findSymbol(@2, "this");
                            $6.insert($6.begin(), thisvar.as<VariablePtr>());
                            if (driver.let_count ==1 ){
                                driver.add_let_lambda(@$, @1, @9, thisvar->getType(), $6, FK_CONSTRUCTOR);
                            }
                            else{
                                RULE $$ = driver.genLambda(@$, $6, thisvar->getType(), $9, FK_CONSTRUCTOR); 
                            }
                            driver.set_inhibit(false);
                        } 
                     ;


lambda_expression : { driver.open_scope(@$,"lambda expr"); } lambda_expression_aux 
                    { driver.close_scope(@$, "lambda expr"); $$ = $2; }
                  ;

%nonassoc "::" ;
%nonassoc ")";
%nonassoc "else";
%right "=>";
%left "spawn" "sync" "syncAll";
%right "->";
%right "?";
%right ":";
%right ".";
%right "[";
%right "catch";
%left "=";
%left ".cast(";
%left ".as(";
%left "==" "!=" "<" "<=" ">" ">=";
%left "+" "-";
%left "*" "/";
%left "&&" "||";
%left "&" "|" "^";
%right "(";
%nonassoc UMINUS;
%nonassoc UDEREF;
%nonassoc BOOL_OP;

%%
// code after the second %% is copyed verbatim at the end of the parser .cpp file

namespace insieme{
namespace core{
namespace parser3{
namespace detail{

    using namespace insieme::core;

    void inspire_parser::error (const location_type& l, const std::string& m) {
      driver.error (l, m);
    } 


} // detail
} // parser3
} // core
} // insieme


