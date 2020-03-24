#ifndef __PJSIP_SIMPLE_CIDF_H__
#define __PJSIP_SIMPLE_CIDF_H__

/**
 * @file cidf.h
 * @brief CIDF/Conference Information Data Format (RFC 4575)
 */
#include <pjsip-simple/types.h>
#include <pjlib-util/xml.h>

PJ_BEGIN_DECL


/**
 * @defgroup PJSIP_SIMPLE_CIDF CIDF/Conference Information Data Format (RFC 4575)
 * @ingroup PJSIP_SIMPLE
 * @brief Support for CIDF/Conference Information Data Format (RFC 4575)
 * @{
 *
 * This file provides tools for manipulating Conference Information Data 
 * Format (CIDF) as described in RFC 4575.
 */
typedef struct pj_xml_node pjcidf_conf;
typedef struct pj_xml_node pjcidf_user;

/******************************************************************************
 * Top level API for managing presence document. 
 *****************************************************************************/
PJ_DECL(pjcidf_conf*)    pjcidf_create(pj_pool_t *pool, const pj_str_t *entity, unsigned version, const pj_str_t *state);
PJ_DECL(pjcidf_conf*)	 pjcidf_parse(pj_pool_t *pool, char *text, int len);


/******************************************************************************
 * API for managing Conference node.
 *****************************************************************************/
PJ_DECL(void)		 pjcidf_conf_construct(pj_pool_t *pool, pjcidf_conf *conf, const pj_str_t *entity, unsigned version, const pj_str_t *state);

PJ_DECL(unsigned) pjcidf_conf_get_version(const pjcidf_conf *conf );
PJ_DECL(const pj_str_t*) pjcidf_conf_get_state(const pjcidf_conf *conf );

PJ_DECL(pjcidf_user*)	 pjcidf_conf_add_user(pj_pool_t *pool, pjcidf_conf *conf, const pj_str_t *id, const pj_str_t *display, const pj_str_t *role);
PJ_DECL(pjcidf_user*)	 pjcidf_conf_get_first_user(pjcidf_conf *conf);
PJ_DECL(pjcidf_user*)	 pjcidf_conf_get_next_user(pjcidf_conf *conf, pjcidf_user *u);


/******************************************************************************
 * API for managing User node.
 *****************************************************************************/
PJ_DECL(void)		 pjcidf_user_construct(pj_pool_t *pool, pjcidf_user *u, const pj_str_t *id, const pj_str_t *display, const pj_str_t *role);
PJ_DECL(const pj_str_t*) pjcidf_user_get_id(const pjcidf_user *u );
PJ_DECL(const pj_str_t*) pjcidf_user_get_display(const pjcidf_user *u);
PJ_DECL(const pj_str_t*) pjcidf_user_get_role(const pjcidf_user *u);
PJ_DECL(const pj_str_t*) pjcidf_user_get_state(const pjcidf_user *u);


/**
 * @}
 */


PJ_END_DECL


#endif	/* __PJSIP_SIMPLE_PIDF_H__ */
