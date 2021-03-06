/*
 * Generated by asn1c-0.9.22 (http://lionet.info/asn1c)
 * From ASN.1 module "SUPL-POS-INIT"
 * 	found in "../supl-posinit.asn"
 */

#ifndef _XNavigationModel_H_
#define _XNavigationModel_H_


#include <asn_application.h>

/* Including external dependencies */
#include <NativeInteger.h>
#include <constr_SEQUENCE.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Forward declarations */
    struct SatelliteInfo;

    /* XNavigationModel */
    typedef struct XNavigationModel
    {
        long gpsWeek;
        long gpsToe;
        long nSAT;
        long toeLimit;
        struct SatelliteInfo *satInfo /* OPTIONAL */;
        /*
	 * This type is extensible,
	 * possible extensions are below.
	 */

        /* Context for parsing across buffer boundaries */
        asn_struct_ctx_t _asn_ctx;
    } XNavigationModel_t;

    /* Implementation */
    extern asn_TYPE_descriptor_t asn_DEF_XNavigationModel;

#ifdef __cplusplus
}
#endif

/* Referred external types */
#include "SatelliteInfo.h"

#endif /* _XNavigationModel_H_ */
#include <asn_internal.h>
