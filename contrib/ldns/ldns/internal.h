#ifndef _INTERNAL_H
#define _INTERNAL_H

ldns_edns_option_list *pkt_edns_data2edns_option_list(const ldns_rdf *);

ldns_status svcparam_key2buffer_str(ldns_buffer *, uint16_t);

ldns_status _ldns_rr_new_frm_fp_l_internal(ldns_rr **, FILE *, uint32_t *,
    ldns_rdf **, ldns_rdf **, int *, bool *);

ldns_status dnssec_zone_equip_zonemd(ldns_dnssec_zone *,
    ldns_rr_list *, ldns_key_list *, int);

#endif
