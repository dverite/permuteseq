/*
 * permuteseq.c
 *
 * PostgreSQL extension to manage scalable pseudo-random permutations of sequences. 
 *
 * By Daniel Vérité, 2016-2017. See LICENSE.md
 */

#include <inttypes.h>

#include "postgres.h"
#include "access/hash.h"
#include "c.h"
#include "commands/sequence.h"
#include "executor/executor.h"
#include "fmgr.h"
#if PG_VERSION_NUM >= 100000
#include "utils/fmgrprotos.h"
#endif

PG_MODULE_MAGIC;

/* Define PG_INT64_{MIN,MAX} for older versions of PG includes that lack them */
#ifndef PG_INT64_MIN
#define PG_INT64_MIN	(-INT64CONST(0x7FFFFFFFFFFFFFFF) - 1)
#endif
#ifndef PG_INT64_MAX
#define PG_INT64_MAX	INT64CONST(0x7FFFFFFFFFFFFFFF)
#endif


Datum permute_nextval(PG_FUNCTION_ARGS);
Datum reverse_permute(PG_FUNCTION_ARGS);
Datum range_encrypt_element(PG_FUNCTION_ARGS);
Datum range_decrypt_element(PG_FUNCTION_ARGS);

static int64 cycle_walking_cipher(int64 minval, int64 maxval,
				  int64 value, uint64 key,
				  int direction);

/*
 * Compute the difference between the min and max of the sequence,
 * avoiding an integer overflow.
 * Returns true if at least 4 elements fit in the sequence.
 */
static bool check_sequence_range(int64 minv, int64 maxv)
{
	/* first check the cases when maxv-minv would overflow an int64 */
	if ((minv > 0 && maxv < PG_INT64_MIN + minv) ||
	    (minv < 0 && maxv > PG_INT64_MAX + minv))
	{
		return true;
	}
	else
		return (maxv - minv >= 4-1);
}

PG_FUNCTION_INFO_V1(permute_nextval);

/*
 * Input: a sequence (through its OID) and a 64-bit encryption key.
 * Take the nextval (64-bit integer) and return its associated
 * unique value in the pseudo-random permutation resulting from
 * encrypting the sequence.
 * The output is constrained to the boundaries of the sequence by
 * using a cycle-walking cipher on top of a Feistel network.
 */
Datum
permute_nextval(PG_FUNCTION_ARGS)
{
	Datum seq_oid = PG_GETARG_DATUM(0);
	uint64 crypt_key = PG_GETARG_INT64(1);
	int64 minval, maxval, result, nextval;
	Datum params;
	bool isnull;

	/* Obtain the minimum and maximum values of the sequence.
	   isnull will always be false, no need to test it. */
	params = DirectFunctionCall1(pg_sequence_parameters, seq_oid);
	minval = DatumGetInt64(GetAttributeByNum((HeapTupleHeader)params, 2, &isnull));
	maxval = DatumGetInt64(GetAttributeByNum((HeapTupleHeader)params, 3, &isnull));

	/* Make sure that the sequence is large enough */
	if (!check_sequence_range(minval, maxval))
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("sequence too short to encrypt."),
				errhint("The difference between minimum and maximum values should be at least 3.")));
	}

	nextval = DatumGetInt64(DirectFunctionCall1(nextval_oid, seq_oid));

	if (nextval < minval || nextval > maxval)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("nextval of the sequence is outside the interval.")));
	}

	result = cycle_walking_cipher(minval,
				      maxval,
				      nextval,
				      crypt_key,
				      0);

	PG_RETURN_INT64(result);
}

PG_FUNCTION_INFO_V1(reverse_permute);

/*
 * Input: a sequence OID, a 64-bit value previously produced by permute_nextval(),
 * and the encryption key
 * Output: the original value of the sequence element
 * The sequence is used to obtain the minval and maxval for the
 * Feistel Network block size and the cycle walking algorithm.
 */
Datum
reverse_permute(PG_FUNCTION_ARGS)
{
	Datum seq_oid = PG_GETARG_DATUM(0);
	int64 value = PG_GETARG_INT64(1);
	uint64 crypt_key = PG_GETARG_INT64(2);
	int64 minval, maxval, result;
	Datum params;
	bool isnull;

	/* Obtain the minimum and maximum values of the sequence.
	   isnull will always be false, no need to test it. */
	params = DirectFunctionCall1(pg_sequence_parameters, seq_oid);
	minval = DatumGetInt64(GetAttributeByNum((HeapTupleHeader)params, 2, &isnull));
	maxval = DatumGetInt64(GetAttributeByNum((HeapTupleHeader)params, 3, &isnull));

	if (maxval - minval < 4)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("sequence too short to decrypt."),
				errhint("The difference between minimum and maximum values should be at least 4.")));
	}

	if (value < minval || value > maxval)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("value out of sequence bounds.")));
	}

	result = cycle_walking_cipher(minval,
				      maxval,
				      value,
				      crypt_key,
				      1);

	PG_RETURN_INT64(result);
}

PG_FUNCTION_INFO_V1(range_encrypt_element);

/*
 * Direct interface to cycle_walking_cipher to encrypt/permute a value
 * from [minval,maxval] into itself, without a database sequence.
 */
Datum
range_encrypt_element(PG_FUNCTION_ARGS)
{
	int64 clearval = PG_GETARG_INT64(0);
	int64 minval = PG_GETARG_INT64(1);
	int64 maxval = PG_GETARG_INT64(2);
	uint64 crypt_key = PG_GETARG_INT64(3);

	if (clearval < minval || clearval > maxval)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("invalid value: %"PRId64" is outside of range [%"PRId64",%"PRId64"]",
				       clearval, minval, maxval)));
	}

	PG_RETURN_INT64(cycle_walking_cipher(minval,
					     maxval,
					     clearval,
					     crypt_key,
					     0));
}

PG_FUNCTION_INFO_V1(range_decrypt_element);

/*
 * Direct interface to cycle_walking_cipher to decrypt/permute a value
 * from [minval,maxval] into itself, without a database sequence.
 */
Datum
range_decrypt_element(PG_FUNCTION_ARGS)
{
	int64 val = PG_GETARG_INT64(0);
	int64 minval = PG_GETARG_INT64(1);
	int64 maxval = PG_GETARG_INT64(2);
	uint64 crypt_key = PG_GETARG_INT64(3);

	if (val < minval || val > maxval)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("invalid value: %"PRId64" is outside of range [%"PRId64",%"PRId64"]",
				       val, minval, maxval)));
	}

	PG_RETURN_INT64(cycle_walking_cipher(minval,
					     maxval,
					     val,
					     crypt_key,
					     1));
}


/*
 * Feistel network with cycle walking loop to produce a encrypted or
 * decrypted result between minval and maxval.
 *
 * direction: 0: encrypt, 1: decrypt
 */
static int64
cycle_walking_cipher(int64 minval, int64 maxval, int64 value, uint64 crypt_key, int direction)
{
	/* Arbitrary maximum number of "walks" along the results
	   searching for a value inside the [minval,maxval] range.
	   It's mainly to avoid an infinite loop in case the chain of
	   results has a cycle (which would imply a bug somewhere). */
	const int walk_max = 1000000;

	/* Half block size */
	unsigned int hsz;

	/* Number of possible values for the output */
	uint64 interval = maxval - minval + 1;
	uint32 mask, Ki;

	/* Number of rounds of the Feistel Network. Must be at least 3. */
	const int NR = 9;

	uint32 l1, r1, l2, r2;
	int walk_count = 0;
	int i;
	uint64 result;		/* offset into the interval */

	/* Compute the half block size: it's the smallest power of 2 such as two
	   blocks are greater than or equal to the size of interval in bits. The
	   half-blocks have equal lengths. */
	hsz = 1;
	while (hsz < 32 && ((uint64)1<<(2*hsz)) < interval)
		hsz++;

	mask = (1 << hsz) - 1;

	/* Scramble the key. This is not strictly necessary, but will
	   help if the user-supplied key is weak, for instance with only a
	   few right-most bits set. */
	crypt_key = hash_uint32(crypt_key & 0xffffffff) |
		((uint64)hash_uint32((crypt_key >> 32) & 0xffffffff)) << 32;

	/* Initialize the two half blocks.
	   Work with the offset into the interval rather than the actual value.
	   This allows to use the full 32-bit range. */
	l1 = (value - minval) >> hsz;
	r1 = (value - minval) & mask;

	do			/* cycle walking */
	{
		for (i = 0; i < NR; i++) /* Feistel network */
		{
			l2 = r1;
			/* The subkey Ki for the round i is a sliding and cycling window
			   of hsz bits over K, moving left to right, so each round takes
			   different bits out of the crypt key. The round function is
			   simply hash(Ri) XOR hash(Ki).
			   When decrypting, Ki corresponds to the Kj of encryption with
			   j=(NR-1-i), i.e. we iterate over subkeys in the reverse order. */
			Ki = crypt_key >> ((hsz* (direction==0 ? i : NR-1-i))&0x3f);
			Ki += (direction==0 ? i : NR-1-i);
			r2 = (l1 ^ DatumGetUInt32(hash_uint32(r1))
			         ^ DatumGetUInt32(hash_uint32(Ki))
			      ) & mask;
			l1 = l2;
			r1 = r2;
		}
		result = ((uint64)r1 << hsz) | l1;
		/* swap one more time to prepare for the next cycle */
		l1 = r2;
		r1 = l2;
	} while ((result > maxval - minval) && walk_count++ < walk_max);

	if (walk_count >= walk_max)
	{
		ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("infinite cycle walking prevented for value %"PRId64" (%d loops)",
				       value, walk_max)));
		
	}

	/* Convert the offset in the interval to an absolute value, possibly negative. */
	return minval + result;
}
