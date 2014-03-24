/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2001-2002  Nokia Corporation
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2002-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2002-2003  Stephen Crane <steve.crane@rococosoft.com>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <glib.h>

#include "src/shared/util.h"
#include "sdpd.h"
#include "log.h"

static sdp_record_t *server = NULL;
static uint32_t fixed_dbts = 0;

/*
 * List of version numbers supported by the SDP server.
 * Add to this list when newer versions are supported.
 */
static sdp_version_t sdpVnumArray[1] = {
	{ 1, 0 }
};
static const int sdpServerVnumEntries = 1;

/*
 * A simple function which returns the time of day in
 * seconds. Used for updating the service db state
 * attribute of the service record of the SDP server
 */
uint32_t sdp_get_time(void)
{
	/*
	 * To handle failure in gettimeofday, so an old
	 * value is returned and service does not fail
	 */
	static struct timeval tm;

	gettimeofday(&tm, NULL);
	return (uint32_t) tm.tv_sec;
}

/*
 * The service database state is an attribute of the service record
 * of the SDP server itself. This attribute is guaranteed to
 * change if any of the contents of the service repository
 * changes. This function updates the timestamp of value of
 * the svcDBState attribute
 * Set the SDP server DB. Simply a timestamp which is the marker
 * when the DB was modified.
 */
static void update_db_timestamp(void)
{
	if (fixed_dbts) {
		sdp_data_t *d = sdp_data_alloc(SDP_UINT32, &fixed_dbts);
		sdp_attr_replace(server, SDP_ATTR_SVCDB_STATE, d);
	} else {
		uint32_t dbts = sdp_get_time();
		sdp_data_t *d = sdp_data_alloc(SDP_UINT32, &dbts);
		sdp_attr_replace(server, SDP_ATTR_SVCDB_STATE, d);
	}
}

void set_fixed_db_timestamp(uint32_t dbts)
{
	fixed_dbts = dbts;
}

void register_public_browse_group(void)
{
	sdp_list_t *browselist;
	uuid_t bgscid, pbgid;
	sdp_data_t *sdpdata;
	sdp_record_t *browse = sdp_record_alloc();

	browse->handle = SDP_SERVER_RECORD_HANDLE + 1;

	sdp_record_add(BDADDR_ANY, browse);
	sdpdata = sdp_data_alloc(SDP_UINT32, &browse->handle);
	sdp_attr_add(browse, SDP_ATTR_RECORD_HANDLE, sdpdata);

	sdp_uuid16_create(&bgscid, BROWSE_GRP_DESC_SVCLASS_ID);
	browselist = sdp_list_append(0, &bgscid);
	sdp_set_service_classes(browse, browselist);
	sdp_list_free(browselist, 0);

	sdp_uuid16_create(&pbgid, PUBLIC_BROWSE_GROUP);
	sdp_attr_add_new(browse, SDP_ATTR_GROUP_ID,
				SDP_UUID16, &pbgid.value.uuid16);
}

/*
 * The SDP server must present its own service record to
 * the service repository. This can be accessed by service
 * discovery clients. This method constructs a service record
 * and stores it in the repository
 */
void register_server_service(void)
{
	sdp_list_t *classIDList;
	uuid_t classID;
	void **versions, **versionDTDs;
	uint8_t dtd;
	sdp_data_t *pData;
	int i;

	server = sdp_record_alloc();
	server->pattern = NULL;

	/* Force the record to be SDP_SERVER_RECORD_HANDLE */
	server->handle = SDP_SERVER_RECORD_HANDLE;

	sdp_record_add(BDADDR_ANY, server);
	sdp_attr_add(server, SDP_ATTR_RECORD_HANDLE,
				sdp_data_alloc(SDP_UINT32, &server->handle));

	sdp_uuid16_create(&classID, SDP_SERVER_SVCLASS_ID);
	classIDList = sdp_list_append(0, &classID);
	sdp_set_service_classes(server, classIDList);
	sdp_list_free(classIDList, 0);

	/*
	 * Set the version numbers supported, these are passed as arguments
	 * to the server on command line. Now defaults to 1.0
	 * Build the version number sequence first
	 */
	versions = malloc(sdpServerVnumEntries * sizeof(void *));
	versionDTDs = malloc(sdpServerVnumEntries * sizeof(void *));
	dtd = SDP_UINT16;
	for (i = 0; i < sdpServerVnumEntries; i++) {
		uint16_t *version = malloc(sizeof(uint16_t));
		*version = sdpVnumArray[i].major;
		*version = (*version << 8);
		*version |= sdpVnumArray[i].minor;
		versions[i] = version;
		versionDTDs[i] = &dtd;
	}
	pData = sdp_seq_alloc(versionDTDs, versions, sdpServerVnumEntries);
	for (i = 0; i < sdpServerVnumEntries; i++)
		free(versions[i]);
	free(versions);
	free(versionDTDs);
	sdp_attr_add(server, SDP_ATTR_VERSION_NUM_LIST, pData);

	update_db_timestamp();
}

void register_device_id(uint16_t source, uint16_t vendor,
					uint16_t product, uint16_t version)
{
	const uint16_t spec = 0x0103;
	const uint8_t primary = 1;
	sdp_list_t *class_list, *group_list, *profile_list;
	uuid_t class_uuid, group_uuid;
	sdp_data_t *sdp_data, *primary_data, *source_data;
	sdp_data_t *spec_data, *vendor_data, *product_data, *version_data;
	sdp_profile_desc_t profile;
	sdp_record_t *record = sdp_record_alloc();

	DBG("Adding device id record for %04x:%04x:%04x:%04x",
					source, vendor, product, version);

	record->handle = sdp_next_handle();

	sdp_record_add(BDADDR_ANY, record);
	sdp_data = sdp_data_alloc(SDP_UINT32, &record->handle);
	sdp_attr_add(record, SDP_ATTR_RECORD_HANDLE, sdp_data);

	sdp_uuid16_create(&class_uuid, PNP_INFO_SVCLASS_ID);
	class_list = sdp_list_append(0, &class_uuid);
	sdp_set_service_classes(record, class_list);
	sdp_list_free(class_list, NULL);

	sdp_uuid16_create(&group_uuid, PUBLIC_BROWSE_GROUP);
	group_list = sdp_list_append(NULL, &group_uuid);
	sdp_set_browse_groups(record, group_list);
	sdp_list_free(group_list, NULL);

	sdp_uuid16_create(&profile.uuid, PNP_INFO_PROFILE_ID);
	profile.version = spec;
	profile_list = sdp_list_append(NULL, &profile);
	sdp_set_profile_descs(record, profile_list);
	sdp_list_free(profile_list, NULL);

	spec_data = sdp_data_alloc(SDP_UINT16, &spec);
	sdp_attr_add(record, 0x0200, spec_data);

	vendor_data = sdp_data_alloc(SDP_UINT16, &vendor);
	sdp_attr_add(record, 0x0201, vendor_data);

	product_data = sdp_data_alloc(SDP_UINT16, &product);
	sdp_attr_add(record, 0x0202, product_data);

	version_data = sdp_data_alloc(SDP_UINT16, &version);
	sdp_attr_add(record, 0x0203, version_data);

	primary_data = sdp_data_alloc(SDP_BOOL, &primary);
	sdp_attr_add(record, 0x0204, primary_data);

	source_data = sdp_data_alloc(SDP_UINT16, &source);
	sdp_attr_add(record, 0x0205, source_data);

	update_db_timestamp();
}

int add_record_to_server(const bdaddr_t *src, sdp_record_t *rec)
{
	sdp_data_t *data;
	sdp_list_t *pattern;

	if (rec->handle == 0xffffffff) {
		rec->handle = sdp_next_handle();
		if (rec->handle < 0x10000)
			return -ENOSPC;
	} else {
		if (sdp_record_find(rec->handle))
			return -EEXIST;
	}

	DBG("Adding record with handle 0x%05x", rec->handle);

	sdp_record_add(src, rec);

	data = sdp_data_alloc(SDP_UINT32, &rec->handle);
	sdp_attr_replace(rec, SDP_ATTR_RECORD_HANDLE, data);

	if (sdp_data_get(rec, SDP_ATTR_BROWSE_GRP_LIST) == NULL) {
		uuid_t uuid;
		sdp_uuid16_create(&uuid, PUBLIC_BROWSE_GROUP);
		sdp_pattern_add_uuid(rec, &uuid);
	}

	for (pattern = rec->pattern; pattern; pattern = pattern->next) {
		char uuid[32];

		if (pattern->data == NULL)
			continue;

		sdp_uuid2strn((uuid_t *) pattern->data, uuid, sizeof(uuid));
		DBG("Record pattern UUID %s", uuid);
	}

	update_db_timestamp();

	return 0;
}

int remove_record_from_server(uint32_t handle)
{
	sdp_record_t *rec;

	/* Refuse to remove the server's own record */
	if (handle == SDP_SERVER_RECORD_HANDLE)
		return -EINVAL;

	DBG("Removing record with handle 0x%05x", handle);

	rec = sdp_record_find(handle);
	if (!rec)
		return -ENOENT;

	if (sdp_record_remove(handle) == 0)
		update_db_timestamp();

	sdp_record_free(rec);

	return 0;
}

/* FIXME: refactor for server-side */
static sdp_record_t *extract_pdu_server(bdaddr_t *device, uint8_t *p,
					unsigned int bufsize,
					uint32_t handleExpected, int *scanned)
{
	int extractStatus = -1, localExtractedLength = 0;
	uint8_t dtd;
	int seqlen = 0;
	sdp_record_t *rec = NULL;
	uint16_t attrId, lookAheadAttrId;
	sdp_data_t *pAttr = NULL;
	uint32_t handle = 0xffffffff;

	*scanned = sdp_extract_seqtype(p, bufsize, &dtd, &seqlen);
	p += *scanned;
	bufsize -= *scanned;

	if (bufsize < sizeof(uint8_t) + sizeof(uint8_t)) {
		SDPDBG("Unexpected end of packet");
		return NULL;
	}

	lookAheadAttrId = get_be16(p + sizeof(uint8_t));

	SDPDBG("Look ahead attr id : %d", lookAheadAttrId);

	if (lookAheadAttrId == SDP_ATTR_RECORD_HANDLE) {
		if (bufsize < (sizeof(uint8_t) * 2) +
					sizeof(uint16_t) + sizeof(uint32_t)) {
			SDPDBG("Unexpected end of packet");
			return NULL;
		}
		handle = get_be32(p + sizeof(uint8_t) + sizeof(uint16_t) +
							sizeof(uint8_t));
		SDPDBG("SvcRecHandle : 0x%x", handle);
		rec = sdp_record_find(handle);
	} else if (handleExpected != 0xffffffff)
		rec = sdp_record_find(handleExpected);

	if (!rec) {
		rec = sdp_record_alloc();
		rec->attrlist = NULL;
		if (lookAheadAttrId == SDP_ATTR_RECORD_HANDLE) {
			rec->handle = handle;
			sdp_record_add(device, rec);
		} else if (handleExpected != 0xffffffff) {
			rec->handle = handleExpected;
			sdp_record_add(device, rec);
		}
	} else {
		sdp_list_free(rec->attrlist, (sdp_free_func_t) sdp_data_free);
		rec->attrlist = NULL;
	}

	while (localExtractedLength < seqlen) {
		int attrSize = sizeof(uint8_t);
		int attrValueLength = 0;

		if (bufsize < attrSize + sizeof(uint16_t)) {
			SDPDBG("Unexpected end of packet: Terminating extraction of attributes");
			break;
		}

		SDPDBG("Extract PDU, sequenceLength: %d localExtractedLength: %d",
							seqlen, localExtractedLength);
		dtd = *(uint8_t *) p;

		attrId = get_be16(p + attrSize);
		attrSize += sizeof(uint16_t);

		SDPDBG("DTD of attrId : %d Attr id : 0x%x", dtd, attrId);

		pAttr = sdp_extract_attr(p + attrSize, bufsize - attrSize,
							&attrValueLength, rec);

		SDPDBG("Attr id : 0x%x attrValueLength : %d", attrId, attrValueLength);

		attrSize += attrValueLength;
		if (pAttr == NULL) {
			SDPDBG("Terminating extraction of attributes");
			break;
		}
		localExtractedLength += attrSize;
		p += attrSize;
		bufsize -= attrSize;
		sdp_attr_replace(rec, attrId, pAttr);
		extractStatus = 0;
		SDPDBG("Extract PDU, seqLength: %d localExtractedLength: %d",
					seqlen, localExtractedLength);
	}

	if (extractStatus == 0) {
		SDPDBG("Successful extracting of Svc Rec attributes");
#ifdef SDP_DEBUG
		sdp_print_service_attr(rec->attrlist);
#endif
		*scanned += seqlen;
	}
	return rec;
}

/*
 * Add the newly created service record to the service repository
 */
int service_register_req(sdp_req_t *req, sdp_buf_t *rsp)
{
	int scanned = 0;
	sdp_data_t *handle;
	uint8_t *p = req->buf + sizeof(sdp_pdu_hdr_t);
	int bufsize = req->len - sizeof(sdp_pdu_hdr_t);
	sdp_record_t *rec;

	req->flags = *p++;
	if (req->flags & SDP_DEVICE_RECORD) {
		bacpy(&req->device, (bdaddr_t *) p);
		p += sizeof(bdaddr_t);
		bufsize -= sizeof(bdaddr_t);
	}

	/* save image of PDU: we need it when clients request this attribute */
	rec = extract_pdu_server(&req->device, p, bufsize, 0xffffffff, &scanned);
	if (!rec)
		goto invalid;

	if (rec->handle == 0xffffffff) {
		rec->handle = sdp_next_handle();
		if (rec->handle < 0x10000) {
			sdp_record_free(rec);
			goto invalid;
		}
	} else {
		if (sdp_record_find(rec->handle)) {
			/* extract_pdu_server will add the record handle
			 * if it is missing. So instead of failing, skip
			 * the record adding to avoid duplication. */
			goto success;
		}
	}

	sdp_record_add(&req->device, rec);
	if (!(req->flags & SDP_RECORD_PERSIST))
		sdp_svcdb_set_collectable(rec, req->sock);

	handle = sdp_data_alloc(SDP_UINT32, &rec->handle);
	sdp_attr_replace(rec, SDP_ATTR_RECORD_HANDLE, handle);

success:
	/* if the browse group descriptor is NULL,
	 * ensure that the record belongs to the ROOT group */
	if (sdp_data_get(rec, SDP_ATTR_BROWSE_GRP_LIST) == NULL) {
		uuid_t uuid;
		sdp_uuid16_create(&uuid, PUBLIC_BROWSE_GROUP);
		sdp_pattern_add_uuid(rec, &uuid);
	}

	update_db_timestamp();

	/* Build a rsp buffer */
	bt_put_be32(rec->handle, rsp->data);
	rsp->data_size = sizeof(uint32_t);

	return 0;

invalid:
	put_be16(SDP_INVALID_SYNTAX, rsp->data);
	rsp->data_size = sizeof(uint16_t);

	return -1;
}

/*
 * Update a service record
 */
int service_update_req(sdp_req_t *req, sdp_buf_t *rsp)
{
	sdp_record_t *orec, *nrec;
	int status = 0, scanned = 0;
	uint8_t *p = req->buf + sizeof(sdp_pdu_hdr_t);
	int bufsize = req->len - sizeof(sdp_pdu_hdr_t);
	uint32_t handle = get_be32(p);

	SDPDBG("Svc Rec Handle: 0x%x", handle);

	p += sizeof(uint32_t);
	bufsize -= sizeof(uint32_t);

	orec = sdp_record_find(handle);

	SDPDBG("SvcRecOld: %p", orec);

	if (!orec) {
		status = SDP_INVALID_RECORD_HANDLE;
		goto done;
	}

	nrec = extract_pdu_server(BDADDR_ANY, p, bufsize, handle, &scanned);
	if (!nrec) {
		status = SDP_INVALID_SYNTAX;
		goto done;
	}

	assert(nrec == orec);

	update_db_timestamp();

done:
	p = rsp->data;
	put_be16(status, p);
	rsp->data_size = sizeof(uint16_t);
	return status;
}

/*
 * Remove a registered service record
 */
int service_remove_req(sdp_req_t *req, sdp_buf_t *rsp)
{
	uint8_t *p = req->buf + sizeof(sdp_pdu_hdr_t);
	uint32_t handle = get_be32(p);
	sdp_record_t *rec;
	int status = 0;

	/* extract service record handle */

	rec = sdp_record_find(handle);
	if (rec) {
		sdp_svcdb_collect(rec);
		status = sdp_record_remove(handle);
		sdp_record_free(rec);
		if (status == 0)
			update_db_timestamp();
	} else {
		status = SDP_INVALID_RECORD_HANDLE;
		SDPDBG("Could not find record : 0x%x", handle);
	}

	p = rsp->data;
	put_be16(status, p);
	rsp->data_size = sizeof(uint16_t);

	return status;
}
