## permuteseq


It's a C PostgreSQL extension to manage scalable pseudo-random permutations of sequences.

## Usage
Example in psql:
```
=# CREATE EXTENSION permuteseq;

=> CREATE SEQUENCE s MINVALUE -10000 MAXVALUE 15000;

=> \set secret_key 123456789012345

=> SELECT  permute_nextval('s'::regclass, :secret_key) FROM generate_series(-10000,-7000);

 permute_nextval 
-----------------
            5676
           11547
           -6199
           14901
            8937
[... skip 2991 unique values in the range [-10000,15000], in a random-looking order ... ]
           -6071
           -7855
           -9299
             255
             944

=> SELECT reverse_permute('s'::regclass, 5676, :secret_key);

 reverse_permute 
-----------------
          -10000

=> SELECT range_encrypt_element(91919191919, 1e10::bigint, 1e11::bigint, :secret_key);

 range_encrypt_element 
-----------------------
           89116043319

=> select range_decrypt_element(89116043319, 1e10::bigint, 1e11::bigint, :secret_key);

 range_decrypt_element 
-----------------------
           91919191919

```

## Functions

### `permute_nextval(seq_oid oid, crypt_key bigint) RETURNS bigint`
Advance sequence and return the new value encrypted within the bounds of the sequence.

### `reverse_permute(seq_oid oid, value bigint, crypt_key bigint) RETURNS bigint`
Compute and return the original clear value from its permuted element in the sequence.

### `range_encrypt_element(clear_val bigint, min_val bigint, max_val bigint, crypt_key bigint) RETURNS bigint`
Encrypt a bigint element in the [min_val,max_val] range with a bigint encryption key.

### `range_decrypt_element(crypt_val bigint, min_val bigint, max_val bigint, crypt_key bigint) RETURNS bigint`
Decrypt a value previously encrypted with `range_encrypt_element()`.

## Installation
The Makefile uses the [PGXS infrastructure](https://www.postgresql.org/docs/current/static/extend-pgxs.html) to find include and library files, and determine the install location.  
Build and install with:
```
$ make
$ (sudo) make install
```


## Some explanations in Q & A form

*Q: How is that better than `ORDER BY random()` applied to a `generate_series` output, or a shuffle algorithm like [Fisher-Yates](https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle)?*  
A: The permuter in this extension does not need to materialize the output sequence, even temporarily.  
It computes any element independantly when needed, in near-constant time,
so it's as efficient with 2^64 elements as with a thousand or a million.

*Q: How about generating `random()`-based integers on the fly and look up for collisions? Isn't that
   simpler?*  
A: It's simpler but doesn't scale. It can do well only when a small number of
elements are actually picked up from a wide sequence.
Otherwise the [Birthday Problem](https://en.wikipedia.org/wiki/Birthday_problem)
kicks in and the exponentially-growing collision probability makes this technique unworkable.

*Q: What are the use cases for permuted sequences?*  
A: Obfuscating synthetic keys, making generators for short
 codes that should not be guessable or enumerable by outsiders: coupon
 numbers, serial numbers, short URLs.

*Q: Why not just use standard 128-bit [Globally unique identifiers](https://en.wikipedia.org/wiki/Globally_unique_identifier) with the `UID` type?*  
A: Sometimes shorter values are necessary. For instance when coupon numbers may be spelled over the phone, `AX8GH4T` is reasonable but `21EC2020-3AEA-4069-A2DD-08002B30309D` is not.

*Q: How to generate several different output sequences from the same input range?*  
A: By using a different 64-bit secret key for each sequence you want to keep secret.

*Q: How does the permuter work, exactly?*  
A: It's essentially a [format-preserving encryption](https://en.wikipedia.org/wiki/Format-preserving_encryption) scheme.  In an inner loop, there's a balanced 9-round [Feistel Cipher](https://en.wikipedia.org/wiki/Feistel_cipher), with a block size determined by the range of the sequence. The round function hashes the current block with bits from the key. A cycle-walking outer loop iterates over that encryption step until the result fits into the desired range, so the outputs are guaranteed to be in the same exact range as the inputs. The reverse permutation follows the same process iterating in the reverse order.

*Q: Is the shuffle effect comparable to crypto-grade randomizing?*  
A: No. Altough it's based on well-known and proven techniques, the limits at play (64-bit key, reduced output space) are too small for that. Also, it's generally safe to assume that code not reviewed by professional cryptographers is not cryptographically strong.  Consider [XTEA](https://en.wikipedia.org/wiki/XTEA), available for PostgreSQL through the [cryptint](http://pgxn.org/dist/cryptint) extension, if you simply want a strong 64-bit to 64-bit Feistel cipher.

*Q: How is the unicity of the output guaranteed?*  
A: By the mathematical property that is at the heart of the [Feistel Network](https://en.wikipedia.org/wiki/Feistel_cipher).

*Q: Are the output sequences deterministic or truly random?*  
A: The permutations are fully deterministic. The random-looking effect is due to encryption, not to a PRNG. The same range with the same encryption key will always produce the same output sequence.

*Q: What happens when the same number is permuted within a different range and the same key?*  
A: It might produce totally a different result, due to the cipher
block size being dynamic and a function of the input range. Precisely, results will
diverge when the closest powers of two to which each range round
up (in size) are different. The dynamic nature of the block size is essential to
reduce the cycle-walking steps.
