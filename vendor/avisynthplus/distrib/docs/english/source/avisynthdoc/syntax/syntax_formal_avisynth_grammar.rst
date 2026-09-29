
Formal AviSynth grammar
=======================

Introduction
~~~~~~~~~~~~

This page presents the formal grammar of the AviSynth script language. This is a dense representation of all the 
rules of the AviSynth script language. While it probably is of more interest to a developer than to an average user, 
it is nevertheless an essential piece of documentation for any programming language and it is thus provided here for 
those more inclined to abstract mathematical reasoning. Have fun!

Before going to the grammar, a few introductory material will be necessary for all those that don't eat bytes 
for breakfast. If you are familiar with BNF / EBNF syntax then skip the following section. 

Background Information
~~~~~~~~~~~~~~~~~~~~~~

Formal grammars of programming and scripting languages are typically written in Backus-Naur Form (BNF) or 
Extended Backus-Naur Form (EBNF) syntax. We have chosen the EBNF syntax because it is easier for human comprehension and 
thus it is a slightly better selection for documentation purposes. The syntax used here follows the ISO/IEC 14977 
Standard, "Extended BNF". The table below summarizes the notation used (infix means that the operator has left 
associativity; postfix that it has right associativity). 


+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
|Extended BNF |Operator|Meaning                                       | Comment                                                                       |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| unquoted    |        |Non-terminal symbol                           | A symbol that is a grouping of low-level symbols (ie not a fundamental one).  |
| words       |        |                                              |                                                                               |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| "..."       |        |Terminal symbol                               | A fundamental (ie not further divisible) symbol of the language.              |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| '...'       |        |Terminal symbol                               | Same as above.                                                                |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| (...)       |        |Brackets                                      | Parentheses just group the symbols inside them in a single (non-terminal)     |
|             |        |                                              | symbol.                                                                       |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| [...]       |        |Optional symbols                              | The symbols inside square braces are optional (ie they are present either 0   |
|             |        |                                              | or 1 times)                                                                   |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| {...}       |        |Symbols repeated zero or more (ie >= 0) times |                                                                               |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| {...}-      |        |Symbols repeated one or more (ie >= 1) times  | Note that the - immediately follows the curly braces.                         |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| =           |infix   |Defining symbol                               | This is the "assignment" operator of EBNF; the left (non-terminal) symbol is  |
|             |        |                                              | (equal to) the right grouping of symbols.                                     |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| ;           |postfix |Rule terminator                               | This operator signals the end of the (assignment) rule (just like in C ; ends |
|             |        |                                              | a statement).                                                                 |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| \|          |infix   |Alternative                                   | Either one of the alternative terminal or non-terminal symbols (and only one) |
|             |        |                                              | will be matched.                                                              |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| ,           |infix   |Concatenation                                 | Symbols on both ends of the , (comma) operator are joined sequentially to     |
|             |        |                                              | form a single (non-terminal) symbol.                                          |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| \-          |infix   |Exception                                     | The effect is the logical negation of the rule following. For example -"a"    |
|             |        |                                              | becomes  ? all characters not equal to a ?.                                   |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| \*          |infix   |Occurences of                                 | The effect of this operator is to repeat the symbol to its right {n} times,   |
|             |        |                                              | where n is the value to its left. For example to state that a (fortran) label |
|             |        |                                              | has exactly 5 characters, one can state: label = 5 * character;.              |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| (\*...\*)   |        |Comment                                       | Arbitrary text documenting something (this is the comment facility of the     |
|             |        |                                              | EBNF language).                                                               |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+
| ?...?       |        |Special sequence                              | Arbitrary text whose interpretation is beyond the scope of the EBNF standard. |
+-------------+--------+----------------------------------------------+-------------------------------------------------------------------------------+

Note: For defining character sequences as terminal symbols one can either use the "string" or 'string' 
facilities of the EBNF language or to use the concatenation operator: character-a , character-b , ..., character-z. 
However for some repetitive tasks such as enumerating all characters of the alphabet or all numeric digits, etc. 
it is common to use a range notation of the form start...end as an extension to the standard. We use it also here. 

The AviSynth Grammar in EBNF Notation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the formulation of the AviSynth grammar below, there are certain items that are not considered part of the grammar 
and thus are considered responsibilities of the tokenizer (to process and strip-off). These are the following:

-   Whitespace.
-   Comments (both single-line and multi-line).
-   Line continuations.
-   The end-of-file condition. 

::

    script =
                { declaration }-
                ;
    declaration =
                statement
                | function_definition 
                ;
    function_definition =
                kw_function , identifier , '(' , [ parameters_list ] , ')' , 
                compound_statement
                ;
    (* optional arguments must come after all positional arguments *)
    parameters_list =
                arguments , ',' , optional_arguments     (* either both types in order *)
                | arguments                              (* or only one (any) of them  *)
                | optional_arguments
                ;
    arguments =
                argument , [ { ',' , argument } ]
                ;
    (* Note: If type_spec is missing, it is implicitly assumed to be: t_val *)
    argument =
                [ type_spec ] , identifier
                ;
    optional_arguments =
                optional_argument , [ { ',' , optional_argument } ]
                ;
    (* Despite the formulation, the tokenizer does not handle whitespace correctly. *)
    (* Example: an optional argument declared simply "int f" parses without error.  *)
    optional_argument =
                [ type_spec ] , quote , identifier , quote
                ;
    type_spec =
                t_val | t_string | t_bool | t_int | t_float | t_clip
                ;
    (* This is probably a parser bug (due to C-origin) because there is only one global *)
    (* function table in AviSynth; it should be  '{' , { statement } , '}'  instead and *)
    (* statement definition below would include function_definition. Then, declaration  *)
    (* would be un-needed as a grammar rule and also: script = { statement }- ;         *)
    compound_statement = 
                '{' , { declaration } , '}'
                ;
    statement =                  (* free-standing compound statements are ?not? allowed *)
                expression
                | try_statement
                | jump_statement
                ;
    try_statement =
                kw_try , compound_statement , 
                kw_catch , '(' , [ identifier ] , ')' , compound_statement
                ;
    jump_statement = 
                kw_return , [ expression ]
                ;
    (* Although expression has only one subtype, keep as a separate production rule *)
    (* for documentation and for easier update of the grammar if extended at future. *)
    expression =
                assignment_exp
                ;
    assignment_exp =
                conditional_exp
                | [ kw_global ] , identifier , '=' , assignment_exp
                ;
    conditional_exp =
                logical_or_exp
                | logical_or_exp , '?' , expression , ':' , conditional_exp
                ;
    logical_or_exp =
                logical_and_exp
                | logical_or_exp , '||' , logical_and_exp
                ;
    logical_and_exp =
                equality_exp
                | logical_and_exp , '&&' , equality_exp
                ;
    equality_exp =
                relational_exp
                | equality_exp , equ_binary_operator , relational_exp
                ;
    equ_binary_operator = 
                '==' | '!=' | '<>'
                ;
    relational_exp =
                additive_exp
                | relational_exp , rel_binary_operator , additive_exp
                ;
    rel_binary_operator = 
                '<' | '>' | '<=' | '>='
                ;
    additive_exp =
                multiplicative_exp
                | additive_exp , add_binary_operator , multiplicative_exp
                ;
    add_binary_operator = 
                '+' | '-' | '++'                               (* ++ is for clips *)
                ;
    multiplicative_exp = 
                unary_exp
                | multiplicative_exp , mul_binary_operator , unary_exp
                ;
    mul_binary_operator = 
                '*' |  '/' |  '%'
                ;
    unary_exp = 
                [ unary_operator ] , postfix_exp
                ;
    unary_operator = 
                sign | '!'
                ;
    (* Because OOP notation simply puts the 1st argument of a function in front of its call *)
    (* it can be chained to all alternatives of primary_exp; therefore this is its place    *)
    postfix_exp = 
                primary_exp
                | function_call
                | primary_exp , { '.' , function_call }-       (* the OOP notation *)
                ;
    function_call =
                identifier , [ '(' , [ argument_exp_list ] , ')' ]
                ;
    (* Assignment is allowed only to optional arguments, *)
    (* which must come after all positional arguments    *)
    argument_exp_list = 
                positional_arg_list , ',' , optional_arg_list  (* either both types in order *)
                | positional_arg_list                          (* or only one (any) of them  *)
                | optional_arg_list
                ;
    positional_arg_list = 
                expression
                | positional_arg_list , ',' , expression
                ;
    optional_arg_list = 
                identifier , '=' , expression
                | optional_arg_list , ',' , identifier , '=' , expression
                ;
    primary_exp =
                constant
                | identifier
                | '(' , expression , ')'
                ;
    identifier = 
                ( letter | "_" )  , { letter | digit | "_" }
                ;
    constant = 
                integer_constant | float_constant | boolean_constant | stringliteral
                ;
    stringliteral = 
                quote , { -quote } , quote | tripleqouote , { -tripleqouote } , tripleqouote
                ;
    boolean_constant =
                true | false | yes | no
                ;
    integer_constant = 
                decimalinteger | hexinteger
                ;
    float_constant = 
                [ sign ] , ( [ intpart ] , fraction | intpart , '.' )
                ;
    decimalinteger = 
                [ sign ] , ( nzero_digit , { digit } | '0' )
                ;
    hexinteger = 
                "$" , { hexdigit }-
                ;
    fraction = 
                '.' , intpart
                ;
    intpart = 
                { digit }-
                ;
    hexdigit = 
                digit | 'a'...'f' | 'A'...'F' 
                ;
    letter = 
                'a'...'z' | 'A'...'Z' 
                ;
    digit = 
                '0' | nzero_digit
                ;
    nzero_digit =
                '1'...'9'
                ;
    sign =
                '-' | '+'
                ;
    
    quote       = '"'   ;
    triplequote = '"""' ;
    
    true        = i_t , i_r , i_u , i_e ;
    false       = i_f , i_a , i_l , i_s , i_e ;
    yes         = i_y , i_e , i_s ;
    no          = i_n , i_o ;
    
    t_val       = i_v , i_a , i_l ;
    t_string    = i_s , i_t , i_r , i_i , i_n , i_g ;
    t_bool      = i_b , i_o , i_o , i_l ;
    t_int       = i_i , i_n , i_t ;
    t_float     = i_f , i_l , i_o , i_a , i_t ;
    t_clip      = i_c , i_l , i_i , i_p ;
    
    kw_function = i_f , i_u , i_n , i_c , i_t , i_i , i_o , i_n ;
    kw_try      = i_t , i_r , i_y ;
    kw_catch    = i_c , i_a , i_t , i_c , i_h ;
    kw_global   = i_g , i_l , i_o , i_b , i_a , i_l ;
    kw_return   = i_r , i_e , i_t , i_u , i_r , i_n ;
    
    i_a = ( 'a' | 'A' ) ;
    i_b = ( 'b' | 'B' ) ;
    i_c = ( 'c' | 'C' ) ;
    i_e = ( 'e' | 'E' ) ;
    i_f = ( 'f' | 'F' ) ;
    i_g = ( 'g' | 'G' ) ;
    i_h = ( 'h' | 'H' ) ;
    i_i = ( 'i' | 'I' ) ;
    i_l = ( 'l' | 'L' ) ;
    i_n = ( 'n' | 'N' ) ;
    i_o = ( 'o' | 'O' ) ;
    i_p = ( 'p' | 'P' ) ;
    i_r = ( 'r' | 'R' ) ;
    i_s = ( 's' | 'S' ) ;
    i_t = ( 't' | 'T' ) ;
    i_u = ( 'u' | 'U' ) ;
    i_v = ( 'v' | 'V' ) ;
    i_y = ( 'y' | 'Y' ) ;
    

-------

Back to: :doc:`Avisynth: the full grammar <syntax_the_full_grammar>`

Back to: :doc:`Avisynth syntax <syntax>`

Back to: :doc:`Avisynth syntax old ref. <syntax_ref>`

This page is originated from `Avisynth.nl: The Format Avisynth Grammar <http://avisynth.nl/index.php/Formal_AviSynth_grammar>`_


$Date: 2024/01/09 10:00:00 $
