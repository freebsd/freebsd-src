/** @file
  Hypertext Transfer Protocol -- HTTP/1.1 Standard definitions, from RFC 2616

  This file contains common HTTP 1.1 definitions from RFC 2616

  (C) Copyright 2015-2016 Hewlett Packard Enterprise Development LP<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef __HTTP_11_H__
#define __HTTP_11_H__

#pragma pack(1)

///
/// HTTP Version (currently HTTP 1.1)
///
/// The version of an HTTP message is indicated by an HTTP-Version field
/// in the first line of the message.
///
#define HTTP_VERSION        "HTTP/1.1"

///
/// HTTP Request Method definitions
///
/// The Method  token indicates the method to be performed on the
/// resource identified by the Request-URI. The method is case-sensitive.
///
#define HTTP_METHOD_OPTIONS "OPTIONS"
#define HTTP_METHOD_GET     "GET"
#define HTTP_METHOD_HEAD    "HEAD"
#define HTTP_METHOD_POST    "POST"
#define HTTP_METHOD_PUT     "PUT"
#define HTTP_METHOD_DELETE  "DELETE"
#define HTTP_METHOD_TRACE   "TRACE"
#define HTTP_METHOD_CONNECT "CONNECT"
#define HTTP_METHOD_PATCH   "PATCH"

///
/// Connect method has maximum length according to EFI_HTTP_METHOD defined in
/// UEFI2.5 spec so use this.
///
#define HTTP_METHOD_MAXIMUM_LEN  sizeof (HTTP_METHOD_CONNECT)

///
/// Accept Request Header
/// The Accept request-header field can be used to specify certain media types which are
/// acceptable for the response. Accept headers can be used to indicate that the request
/// is specifically limited to a small set of desired types, as in the case of a request
/// for an in-line image.
///
#define HTTP_HEADER_ACCEPT             "Accept"


///
/// Accept-Charset Request Header
/// The Accept-Charset request-header field can be used to indicate what character sets
/// are acceptable for the response. This field allows clients capable of understanding
/// more comprehensive or special-purpose character sets to signal that capability to a
/// server which is capable of representing documents in those character sets.
///
#define HTTP_HEADER_ACCEPT_CHARSET     "Accept-Charset"

///
/// Accept-Language Request Header
/// The Accept-Language request-header field is similar to Accept,
/// but restricts the set of natural languages that are preferred
/// as a response to the request.
///
#define HTTP_HEADER_ACCEPT_LANGUAGE    "Accept-Language"

///
/// Accept-Ranges Request Header
/// The Accept-Ranges response-header field allows the server to
/// indicate its acceptance of range requests for a resource:
///
#define HTTP_HEADER_ACCEPT_RANGES      "Accept-Ranges"


///
/// Accept-Encoding Request Header
/// The Accept-Encoding request-header field is similar to Accept,
/// but restricts the content-codings that are acceptable in the response.
///
#define HTTP_HEADER_ACCEPT_ENCODING    "Accept-Encoding"

///
/// Content-Encoding Header
/// The Content-Encoding entity-header field is used as a modifier to the media-type.
/// When present, its value indicates what additional content codings have been applied
/// to the entity-body, and thus what decoding mechanisms must be applied in order to
/// obtain the media-type referenced by the Content-Type header field. Content-Encoding
/// is primarily used to allow a document to be compressed without losing the identity
/// of its underlying media type.
///
#define HTTP_HEADER_CONTENT_ENCODING   "Content-Encoding"

///
/// HTTP Content-Encoding Compression types
///

#define HTTP_CONTENT_ENCODING_IDENTITY "identity"  /// No transformation is used. This is the default value for content coding.
#define HTTP_CONTENT_ENCODING_GZIP     "gzip"      /// Content-Encoding: GNU zip format (described in RFC 1952).
#define HTTP_CONTENT_ENCODING_COMPRESS "compress"  /// encoding format produced by the common UNIX file compression program "compress".
#define HTTP_CONTENT_ENCODING_DEFLATE  "deflate"   /// The "zlib" format defined in RFC 1950 in combination with the "deflate"
                                                   /// compression mechanism described in RFC 1951.


///
/// Content-Type Header
/// The Content-Type entity-header field indicates the media type of the entity-body sent to
/// the recipient or, in the case of the HEAD method, the media type that would have been sent
/// had the request been a GET.
///
#define HTTP_HEADER_CONTENT_TYPE       "Content-Type"
//
// Common Media Types defined in http://www.iana.org/assignments/media-types/media-types.xhtml
//
#define HTTP_CONTENT_TYPE_APP_JSON          "application/json"
#define HTTP_CONTENT_TYPE_APP_OCTET_STREAM  "application/octet-stream"

#define HTTP_CONTENT_TYPE_TEXT_HTML         "text/html"
#define HTTP_CONTENT_TYPE_TEXT_PLAIN        "text/plain"
#define HTTP_CONTENT_TYPE_TEXT_CSS          "text/css"
#define HTTP_CONTENT_TYPE_TEXT_XML          "text/xml"

#define HTTP_CONTENT_TYPE_IMAGE_GIF         "image/gif"
#define HTTP_CONTENT_TYPE_IMAGE_JPEG        "image/jpeg"
#define HTTP_CONTENT_TYPE_IMAGE_PNG         "image/png"
#define HTTP_CONTENT_TYPE_IMAGE_SVG_XML     "image/svg+xml"


///
/// Content-Length Header
/// The Content-Length entity-header field indicates the size of the entity-body,
/// in decimal number of OCTETs, sent to the recipient or, in the case of the HEAD
/// method, the size of the entity-body that would have been sent had the request been a GET.
///
#define HTTP_HEADER_CONTENT_LENGTH     "Content-Length"

///
/// Transfer-Encoding Header
/// The Transfer-Encoding general-header field indicates what (if any) type of transformation
/// has been applied to the message body in order to safely transfer it between the sender
/// and the recipient. This differs from the content-coding in that the transfer-coding
/// is a property of the message, not of the entity.
///
#define HTTP_HEADER_TRANSFER_ENCODING  "Transfer-Encoding"


///
/// User Agent Request Header
///
/// The User-Agent request-header field contains information about the user agent originating
/// the request. This is for statistical purposes, the tracing of protocol violations, and
/// automated recognition of user agents for the sake of tailoring responses to avoid
/// particular user agent limitations. User agents SHOULD include this field with requests.
/// The field can contain multiple product tokens and comments identifying the agent and any
/// subproducts which form a significant part of the user agent.
/// By convention, the product tokens are listed in order of their significance for
/// identifying the application.
///
#define HTTP_HEADER_USER_AGENT         "User-Agent"

///
/// Host Request Header
///
/// The Host request-header field specifies the Internet host and port number of the resource
/// being requested, as obtained from the original URI given by the user or referring resource
///
#define HTTP_HEADER_HOST              "Host"

///
/// Location Response Header
///
/// The Location response-header field is used to redirect the recipient to a location other than
/// the Request-URI for completion of the request or identification of a new resource.
/// For 201 (Created) responses, the Location is that of the new resource which was created by
/// the request. For 3xx responses, the location SHOULD indicate the server's preferred URI for
/// automatic redirection to the resource. The field value consists of a single absolute URI.
///
#define HTTP_HEADER_LOCATION           "Location"

///
/// The If-Match request-header field is used with a method to make it conditional.
/// A client that has one or more entities previously obtained from the resource
/// can verify that one of those entities is current by including a list of their
/// associated entity tags in the If-Match header field.
/// The purpose of this feature is to allow efficient updates of cached information
/// with a minimum amount of transaction overhead. It is also used, on updating requests,
/// to prevent inadvertent modification of the wrong version of a resource.
/// As a special case, the value "*" matches any current entity of the resource.
///
#define HTTP_HEADER_IF_MATCH          "If-Match"


///
/// The If-None-Match request-header field is used with a method to make it conditional.
/// A client that has one or more entities previously obtained from the resource can verify
/// that none of those entities is current by including a list of their associated entity
/// tags in the If-None-Match header field. The purpose of this feature is to allow efficient
/// updates of cached information with a minimum amount of transaction overhead. It is also used
/// to prevent a method (e.g. PUT) from inadvertently modifying an existing resource when the
/// client believes that the resource does not exist.
///
#define HTTP_HEADER_IF_NONE_MATCH     "If-None-Match"



///
/// Authorization Request Header
/// The Authorization field value consists of credentials
/// containing the authentication information of the user agent for
/// the realm of the resource being requested.
///
#define HTTP_HEADER_AUTHORIZATION     "Authorization"

///
/// ETAG Response Header
/// The ETag response-header field provides the current value of the entity tag
/// for the requested variant.
///
#define HTTP_HEADER_ETAG              "ETag"

///
/// Custom header field checked by the iLO web server to
/// specify a client session key.
/// Example:     X-Auth-Token: 24de6b1f8fa147ad59f6452def628798
///
#define  HTTP_HEADER_X_AUTH_TOKEN      "X-Auth-Token"

///
/// Expect Header
/// The "Expect" header field in a request indicates a certain set of
/// behaviors (expectations) that need to be supported by the server in
/// order to properly handle this request. The only such expectation
/// defined by this specification is 100-continue.
///
#define  HTTP_HEADER_EXPECT            "Expect"

///
/// Expect Header Value
///
#define  HTTP_EXPECT_100_CONTINUE       "100-continue"

#pragma pack()

#endif
