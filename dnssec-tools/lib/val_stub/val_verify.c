/*
 * Copyright 2005 Sparta, Inc.  All rights reserved.
 * See the COPYING file distributed with this software for details.
 *
 * Author: Abhijit Hayatnagarkar
 *
 * This is the implementation file for the verifier.
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "val_print.h"
#include "val_parse.h"
#include "crypto/val_rsamd5.h"
#include "crypto/val_rsasha1.h"
#include "crypto/val_dsasha1.h"

#include "val_verify.h"

#define ZONE_KEY_FLAG 0x0100 /* Zone Key Flag, RFC 4034 */
#define BUFLEN 8192

/* Verify a signature, given the data and the dnskey */
static int val_sigverify (const char *data,
			  int data_len,
			  const val_dnskey_rdata_t dnskey,
			  const val_rrsig_rdata_t rrsig)
{
    /* Check if the dnskey is a zone key */
    if ((dnskey.flags & ZONE_KEY_FLAG) == 0) {
	printf("DNSKEY not a zone signing key\n");
	return INDETERMINATE;
    }
    
    /* Check dnskey protocol value */
    if (dnskey.protocol != 3) {
	printf("Invalid protocol field in DNSKEY record: %d\n",
	       dnskey.protocol);
	return INDETERMINATE;
    }

    /* Match dnskey and rrsig algorithms */
    if (dnskey.algorithm != rrsig.algorithm) {
	printf("Algorithm mismatch between DNSKEY (%d) and RRSIG (%d) records.\n",
	       dnskey.algorithm, rrsig.algorithm);
	return INDETERMINATE;
    }

    /* Check signature inception and expiration times */
    struct timeval tv;
    struct timezone tz;
    gettimeofday(&tv, &tz);
    if (tv.tv_sec < rrsig.sig_incp) {
	printf("Signature not yet valid. Current time (%s) is less than signature inception time (%s).\n",
	       ctime((const time_t *)(&(tv.tv_sec))), ctime((const time_t *)(&(rrsig.sig_incp))));
	return INDETERMINATE;
    }

    if (tv.tv_sec > rrsig.sig_expr) {
	printf("Signature expired. Current time (%s) is greater than signature expiration time (%s).\n",
	       ctime((const time_t *)(&(tv.tv_sec))), ctime((const time_t *)(&(rrsig.sig_expr))));
	return INDETERMINATE;
    }

    switch(rrsig.algorithm) {
	
    case 1: return  rsamd5_sigverify(data, data_len, dnskey, rrsig); break;
    case 3: return dsasha1_sigverify(data, data_len, dnskey, rrsig); break;
    case 5: return rsasha1_sigverify(data, data_len, dnskey, rrsig); break;

    default:
	do {
	    printf("Unsupported algorithm %d.\n",
		   dnskey.algorithm);
	    return INDETERMINATE;
	} while (0);
    }

}


val_result_t val_verify (struct val_context *context, struct domain_info *response) {
    val_dnskey_rdata_t *dnskey_rdata, *dp;
    struct rrset_rec *dnskeys;
    struct rrset_rec *rrset;
    u_int8_t sig_data[BUFLEN*2];
    val_result_t status = INDETERMINATE;

    if (!response) {
	printf("val_verify(): no response to verify\n");
	return INDETERMINATE;
    }

    dnskeys = context->learned_keys;
    if (!dnskeys) {
	printf("val_verify(): no dnskeys found\n");
	return INDETERMINATE;
    }

    // Parse the dnskeys
    dnskey_rdata = NULL;
    printf("val_verify(): parsing DNSKEYs\n");

    while (dnskeys) {
	if (dnskeys->rrs_type_h == ns_t_dnskey) {
	    struct rr_rec *rrs_data = dnskeys->rrs_data;
	    while (rrs_data) {
		val_dnskey_rdata_t *new_dnskey_rdata = (val_dnskey_rdata_t *) malloc (sizeof(val_dnskey_rdata_t));
		val_parse_dnskey_rdata (rrs_data->rr_rdata,
					rrs_data->rr_rdata_length_h,
					new_dnskey_rdata);
		
		new_dnskey_rdata->next = dnskey_rdata;
		dnskey_rdata = new_dnskey_rdata;
		val_print_dnskey_rdata("", dnskey_rdata);
		printf("\n");
		
		rrs_data = rrs_data->rr_next;
	    }
	}
	dnskeys = dnskeys->rrs_next;
    }

    if (dnskey_rdata == NULL) {
	// No DNSKEYs were found
	status = DNSKEY_MISSING;
	goto cleanup;
    }

    // Check for each rrset
    rrset = response->di_rrset;
    while(rrset) {
	struct rr_rec *rrs_sig = rrset->rrs_sig;

	// Check for each signature
	while (rrs_sig) {
	    val_rrsig_rdata_t rrsig_rdata;
	    int sig_data_len;

	    printf("val_verify(): parsing rrsig\n");
	    bzero(&rrsig_rdata, sizeof(rrsig_rdata));
	    val_parse_rrsig_rdata(rrs_sig->rr_rdata, rrs_sig->rr_rdata_length_h,
				  &rrsig_rdata);
	    val_print_rrsig_rdata (" ", &rrsig_rdata);
	    printf("\n");

	    if (rrsig_rdata.type_covered != rrset->rrs_type_h) {
		printf("Different type covered by rrsig");
		rrs_sig = rrs_sig->rr_next;
		continue;
	    }

	    // Compose the signature data
	    printf("val_verify(): composing signature data\n");
	    bzero(sig_data, BUFLEN*2);

	    /* Copy rrsig rdata, except signature */
	    /* RFC 4034 section 3.1.7 says that the signer's name field in the rrsig_rdata
	     * is not compressed */
	    memcpy(sig_data, rrs_sig->rr_rdata, rrs_sig->rr_rdata_length_h - rrsig_rdata.signature_len);
	    sig_data_len = rrs_sig->rr_rdata_length_h - rrsig_rdata.signature_len;
	    
	    /* Copy RRs in the rrset in canonical order */
	    // Compose the canonical form of the rrset data
	    printf("val_verify(): canonicalizing rrset\n");
	    {
		unsigned char canon_rrset[BUFLEN];
		int canon_rrset_length = 0;
		bzero(canon_rrset, BUFLEN);

		canon_rrset_length = val_get_canon_rrset(rrset, rrsig_rdata.orig_ttl, canon_rrset, BUFLEN);
		memcpy(sig_data + sig_data_len, canon_rrset, canon_rrset_length);
		sig_data_len += canon_rrset_length;
	    }

	    // For each dnskey verify if the signature matches
	    dp = dnskey_rdata;
	    while (dp) {
		printf("val_verify(): Trying DNSKEY with keytag = %d\n", dp->key_tag);
		if (dp->key_tag != rrsig_rdata.key_tag) {
		    dp = dp->next;
		    printf("val_verify(): keytag does not match. Trying next DNSKEY\n");
		    continue;
		}

		/* verify signature */
		printf("val_verify(): verifying signature\n");
		if ((status = val_sigverify(sig_data, sig_data_len,
				  *dp, rrsig_rdata)) == VALIDATE_SUCCESS) {
		    goto cleanup;
		}
		else {
		    printf("val_verify(): verification failed. Trying next DNSKEY\n");
		}

		dp = dp->next;
	    }

	    rrs_sig = rrs_sig->rr_next;
	}
	
	rrset = rrset->rrs_next;
    }

 cleanup:
    // Free dnskey rdata structs
    dp = dnskey_rdata;
    while (dp) {
	val_dnskey_rdata_t *sdp;
	sdp = dp->next;
	free(dp);
	dp = sdp;
    }
    
    return status;
    
}

