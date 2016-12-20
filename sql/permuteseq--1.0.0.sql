CREATE OR REPLACE FUNCTION permute_nextval(seq_oid oid, crypt_key int8)
RETURNS int8
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT;

COMMENT ON FUNCTION permute_nextval(oid,int8)
IS 'Advance sequence and return the new value encrypted within the bounds of the sequence';

CREATE OR REPLACE FUNCTION reverse_permute(seq_oid oid, value int8, crypt_key int8)
RETURNS int8
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE STRICT;

COMMENT ON FUNCTION reverse_permute(oid,int8,int8)
IS 'Compute and return the original clear value from its permuted element in the sequence';

CREATE OR REPLACE FUNCTION range_encrypt_element(
  clear_val int8, min_val int8, max_val int8, crypt_key int8)
RETURNS int8
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT;

COMMENT ON FUNCTION range_encrypt_element(int8,int8,int8,int8)
IS 'Encrypt a bigint element within the (min,max) range and an int8 key';

CREATE OR REPLACE FUNCTION range_decrypt_element(
  crypt_val int8, min_val int8, max_val int8, crypt_key int8)
RETURNS int8
AS 'MODULE_PATHNAME'
LANGUAGE C IMMUTABLE STRICT;

COMMENT ON FUNCTION range_decrypt_element(int8,int8,int8,int8)
IS 'Decrypt a bigint element encrypted with range_encrypt_element';
