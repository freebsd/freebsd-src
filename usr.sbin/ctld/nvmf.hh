/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#ifndef __NVMF_HH__
#define	__NVMF_HH__

struct nvmf_association_deleter {
	void operator()(struct nvmf_association *na) const
	{
		nvmf_free_association(na);
	}
};

using nvmf_association_up = std::unique_ptr<nvmf_association,
					    nvmf_association_deleter>;

struct nvmf_capsule_deleter {
	void operator()(struct nvmf_capsule *nc) const
	{
		nvmf_free_capsule(nc);
	}
};

using nvmf_capsule_up = std::unique_ptr<nvmf_capsule, nvmf_capsule_deleter>;

struct nvmf_qpair_deleter {
	void operator()(struct nvmf_qpair *qp) const
	{
		nvmf_free_qpair(qp);
	}
};

using nvmf_qpair_up = std::unique_ptr<nvmf_qpair, nvmf_qpair_deleter>;

struct nvmf_portal : public portal {
	nvmf_portal(struct portal_group *pg, const char *listen,
	    portal_protocol protocol, freebsd::addrinfo_up ai,
	    const struct nvmf_association_params &aparams,
	    nvmf_association_up na) :
		portal(pg, listen, protocol, std::move(ai)),
		p_aparams(aparams), p_association(std::move(na)) {}
	virtual ~nvmf_portal() override = default;

	const struct nvmf_association_params *aparams() const
	{ return &p_aparams; }

protected:
	struct nvmf_association *association() { return p_association.get(); }

private:
	struct nvmf_association_params	p_aparams;
	nvmf_association_up		p_association;
};

struct nvmf_discovery_portal final : public nvmf_portal {
	nvmf_discovery_portal(struct portal_group *pg, const char *listen,
	    portal_protocol protocol, freebsd::addrinfo_up ai,
	    const struct nvmf_association_params &aparams,
	    nvmf_association_up na) :
		nvmf_portal(pg, listen, protocol, std::move(ai), aparams,
		    std::move(na)) {}

	void handle_connection(freebsd::fd_up fd, const char *host,
	    const struct sockaddr *client_sa) override;
};

#endif /* !__NVMF_HH__ */
