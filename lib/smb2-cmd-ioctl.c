/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2018 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef STDC_HEADERS
#include <stddef.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <errno.h>

#include "compat.h"

#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-private.h"

static int
smb2_encode_ioctl_request(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_ioctl_request *req)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        len = SMB2_IOCTL_REQUEST_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate ioctl buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);

        smb2_set_uint16(iov, 0, SMB2_IOCTL_REQUEST_SIZE);
        smb2_set_uint32(iov, 4, req->ctl_code);
        memcpy(iov->buf + 8, req->file_id, SMB2_FD_SIZE);
        smb2_set_uint32(iov, 24, SMB2_HEADER_SIZE +
                        (SMB2_IOCTL_REQUEST_SIZE & 0xfffffffe));
        smb2_set_uint32(iov, 28, req->input_count);
        smb2_set_uint32(iov, 32, 0); /* Max input response */
        smb2_set_uint32(iov, 44, 65535); /* Max output response */
        smb2_set_uint32(iov, 48, req->flags);

        if (req->input_count) {
                iov = smb2_add_iovector(smb2, &pdu->out, req->input,
                                        req->input_count, NULL);
        }

        return 0;
}

struct smb2_pdu *
smb2_cmd_ioctl_async(struct smb2_context *smb2,
                     struct smb2_ioctl_request *req,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_IOCTL, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_ioctl_request(smb2, pdu, req)) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        if (smb2_pad_to_64bit(smb2, &pdu->out) != 0) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        return pdu;
}

static int
smb2_encode_ioctl_reply(struct smb2_context *smb2,
                          struct smb2_pdu *pdu,
                          struct smb2_ioctl_reply *rep)
{
        int len;
        uint8_t *buf;
        struct smb2_iovec *iov;

        pdu->header.flags |= SMB2_FLAGS_SERVER_TO_REDIR;
        pdu->header.credit_request_response = 1;

        len = SMB2_IOCTL_REPLY_SIZE & 0xfffffffe;
        buf = calloc(len, sizeof(uint8_t));
        if (buf == NULL) {
                smb2_set_error(smb2, "Failed to allocate ioctl reply buffer");
                return -1;
        }

        iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);

        smb2_set_uint16(iov, 0, SMB2_IOCTL_REPLY_SIZE);
        smb2_set_uint32(iov, 4, rep->ctl_code);
        memcpy(iov->buf + 8, rep->file_id, SMB2_FD_SIZE);
        smb2_set_uint32(iov, 24, SMB2_HEADER_SIZE +
                        (SMB2_IOCTL_REPLY_SIZE & 0xfffffffe));
        smb2_set_uint32(iov, 28, rep->input_count);
        smb2_set_uint32(iov, 32, SMB2_HEADER_SIZE +
                        (SMB2_IOCTL_REPLY_SIZE & 0xfffffffe) +
                        PAD_TO_64BIT(rep->input_count));
        smb2_set_uint32(iov, 36, rep->output_count);
        smb2_set_uint32(iov, 40, rep->flags);

        /*
        if (rep->input_count) {
                len = PAD_TO_64BIT(rep->input_count);
                buf = malloc(len);
                if (buf == NULL) {
                        smb2_set_error(smb2, "Failed to allocate ioctl input");
                        return -1;
                }
                memcpy(buf, rep->input, rep->input_count);
                memset(buf + rep->input_count, 0, len - rep->input_count);
                iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        }
        */
        if (rep->output_count) {
                len = PAD_TO_64BIT(rep->output_count);
                buf = malloc(len);
                if (buf == NULL) {
                        smb2_set_error(smb2, "Failed to allocate ioctl output");
                        return -1;
                }
                memcpy(buf, rep->output, rep->output_count);
                memset(buf + rep->output_count, 0, len - rep->output_count);
                iov = smb2_add_iovector(smb2, &pdu->out, buf, len, free);
        }

        return 0;
}

struct smb2_pdu *
smb2_cmd_ioctl_reply_async(struct smb2_context *smb2,
                     struct smb2_ioctl_reply *rep,
                     smb2_command_cb cb, void *cb_data)
{
        struct smb2_pdu *pdu;

        pdu = smb2_allocate_pdu(smb2, SMB2_IOCTL, cb, cb_data);
        if (pdu == NULL) {
                return NULL;
        }

        if (smb2_encode_ioctl_reply(smb2, pdu, rep)) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        if (smb2_pad_to_64bit(smb2, &pdu->out) != 0) {
                smb2_free_pdu(smb2, pdu);
                return NULL;
        }

        return pdu;
}

#define IOV_OFFSET (rep->output_offset - SMB2_HEADER_SIZE - \
                    (SMB2_IOCTL_REPLY_SIZE & 0xfffe))

int
smb2_process_ioctl_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_ioctl_reply *rep;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        rep = malloc(sizeof(*rep));
        if (rep == NULL) {
                smb2_set_error(smb2, "Failed to allocate ioctl reply");
                return -1;
        }
        pdu->payload = rep;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_IOCTL_REPLY_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Ioctl "
                               "reply. Expected %d, got %d",
                               SMB2_IOCTL_REPLY_SIZE,
                               (int)iov->len);
                return -1;
        }

        smb2_get_uint32(iov, 4, &rep->ctl_code);
        memcpy(rep->file_id, iov->buf + 8, SMB2_FD_SIZE);
        smb2_get_uint32(iov, 32, &rep->output_offset);
        smb2_get_uint32(iov, 36, &rep->output_count);
        smb2_get_uint32(iov, 40, &rep->flags);

        if (rep->output_count == 0) {
                return 0;
        }

        if (rep->output_offset < SMB2_HEADER_SIZE +
            (SMB2_IOCTL_REPLY_SIZE & 0xfffe)) {
                smb2_set_error(smb2, "Output buffer overlaps with "
                               "Ioctl reply header");
                return -1;
        }

        /* Return the amount of data that the output buffer will take up.
         * Including any padding before the output buffer itself.
         */
        return IOV_OFFSET + rep->output_count;
}

int
smb2_process_ioctl_variable(struct smb2_context *smb2,
                            struct smb2_pdu *pdu)
{
        struct smb2_ioctl_reply *rep = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        struct smb2_iovec vec;
        void *ptr;

        if (rep->output_count > iov->len - IOV_OFFSET) {
                return -EINVAL;
        }

        vec.buf = &iov->buf[IOV_OFFSET];
        vec.len = iov->len - IOV_OFFSET;

        switch (rep->ctl_code) {
        case SMB2_FSCTL_GET_REPARSE_POINT:
                ptr = smb2_alloc_init(smb2,
                                      sizeof(struct smb2_reparse_data_buffer));
                if (smb2_decode_reparse_data_buffer(smb2, ptr, ptr, &vec)) {
                        smb2_set_error(smb2, "could not decode reparse "
                                       "data buffer. %s",
                                       smb2_get_error(smb2));
                        return -1;
                }
                break;
        default:
                ptr = smb2_alloc_init(smb2, rep->output_count);
                if (ptr == NULL) {
                        return -ENOMEM;
                }
                memcpy(ptr, &iov->buf[IOV_OFFSET], iov->len - IOV_OFFSET);
        }

        rep->output = ptr;

        return 0;
}

#define IOVREQ_OFFSET (req->input_offset - SMB2_HEADER_SIZE - \
                    (SMB2_IOCTL_REQUEST_SIZE & 0xfffe))

int
smb2_process_ioctl_request_fixed(struct smb2_context *smb2,
                         struct smb2_pdu *pdu)
{
        struct smb2_ioctl_request *req;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];
        uint16_t struct_size;

        req = malloc(sizeof(*req));
        if (req == NULL) {
                smb2_set_error(smb2, "Failed to allocate ioctl request");
                return -1;
        }
        pdu->payload = req;

        smb2_get_uint16(iov, 0, &struct_size);
        if (struct_size != SMB2_IOCTL_REQUEST_SIZE ||
            (struct_size & 0xfffe) != iov->len) {
                smb2_set_error(smb2, "Unexpected size of Ioctl "
                               "request. Expected %d, got %d",
                               SMB2_IOCTL_REQUEST_SIZE,
                               (int)iov->len);
                return -1;
        }

        smb2_get_uint32(iov, 4, &req->ctl_code);
        memcpy(req->file_id, iov->buf + 8, SMB2_FD_SIZE);
        smb2_get_uint32(iov, 24, &req->input_offset);
        smb2_get_uint32(iov, 28, &req->input_count);
        smb2_get_uint32(iov, 32, &req->max_input_response);
        smb2_get_uint32(iov, 40, &req->output_offset);
        smb2_get_uint32(iov, 44, &req->output_count);
        smb2_get_uint32(iov, 48, &req->max_output_response);
        smb2_get_uint32(iov, 52, &req->flags);

        if (req->input_count == 0) {
                return 0;
        }

        if (req->input_offset < SMB2_HEADER_SIZE +
            (SMB2_IOCTL_REQUEST_SIZE & 0xfffe)) {
                smb2_set_error(smb2, "Output buffer overlaps with "
                               "Ioctl request header");
                return -1;
        }

        /* Return the amount of data that the input buffer will take up.
         * Including any padding before the input buffer itself.
         */
        return IOVREQ_OFFSET + req->input_count;
}

int
smb2_process_ioctl_request_variable(struct smb2_context *smb2,
                            struct smb2_pdu *pdu)
{
        struct smb2_ioctl_request *req = pdu->payload;
        struct smb2_iovec *iov = &smb2->in.iov[smb2->in.niov - 1];

        if (req->input_count > iov->len - IOVREQ_OFFSET) {
                return -EINVAL;
        }

        req->input = &iov->buf[IOVREQ_OFFSET];
        return 0;
}

