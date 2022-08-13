TODO supported features:
-- arbitrary number of
---- local int variables
---- shared non-volatile int variables
---- shared volatile int variables
---- locks with lock and unlock
---- operators on variables: + (only binary), - (only binary), *, /, %
---- assignment of variables with =, +=, -=, *=, /=, %=
---- unary post-fix increment/decrement (++, --), only as a statement

statement types:
-- method call: <identifier> . <identifier> ( ) ;
-- assignment: <identifier> <assignment-operator> <expression>
-- increment: <identifier> ++ ;
-- decrement: <identifier> -- ;
