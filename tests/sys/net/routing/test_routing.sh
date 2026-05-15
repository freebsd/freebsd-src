#
# Copyright (c) 2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

. $(atf_get_srcdir)/../../common/vnet.subr

jq_rtentry()
{
	local route="$1"

	jq -r '.statistics."route-information"."route-table"."rt-family".[]."rt-entry".[] |
	    select(.destination == "'${route}'")'
}

jq_nhop_filter()
{
	local nhop="$1"
	local weight="$2"
	local metric="$3"

	jq -r 'select(.gateway == "'${nhop}'") |
	    select(.weight == '${weight}') |
	    select(.metric == '${metric}') |
	    .gateway'
}


atf_test_case "add_lowest_metric" "cleanup"
add_lowest_metric_head()
{
	atf_set descr 'Create 4 routes to same dst and verify the lowest metric wins'
	atf_set require.user root
	atf_set require.progs jq
}

add_lowest_metric_body()
{
	local epair laddr route nhop1 nhop2 nhop3

	laddr="3fff::1"
	route="3fff:a::"
	nhop1="3fff::1"
	nhop2="3fff::2"
	nhop3="3fff::3"

	vnet_init
	epair=$(vnet_mkepair)

	atf_check -o ignore \
	    ifconfig ${epair}a inet6 ${laddr} up

	# Create an ECMP route with metric 2
	atf_check -o ignore \
	    route -6 add -net ${route}/64 -gateway ${nhop2} -weight 10 -metric 2
	atf_check -o ignore \
	    route -6 add -net ${route}/64 -gateway ${nhop3} -weight 10 -metric 2

	# Validate routes
	atf_check -o save:netstat \
	    netstat -rn6 --libxo json
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop2} 10 2)
	atf_check_equal "$output" "$nhop2"
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop3} 10 2)
	atf_check_equal "$output" "$nhop3"

	# Create a route with metric 3
	atf_check -o ignore \
	    route -6 add -net ${route}/64 -gateway ${nhop1} -metric 3
	# Verify that nhop1 is not the best route
	atf_check -o not-match:".*gateway: ${nhop1}.*" \
	    route -n6 get -net ${route}/64

	# Create a route to the same nhop with same metric 3 and verify it fails
	atf_check -s exit:1 -o ignore -e match:".*exists.*" \
	    route -6 add -net ${route}/64 -gateway ${nhop1} -metric 3

	# Create a route to an existing nhop with lower metric
	atf_check -o ignore \
	    route -6 add -net ${route}/64 -gateway ${nhop1} -metric 1
	# Verify that nhop1 is now the best route
	atf_check -o match:".*gateway: ${nhop1}.*" \
	    route -n6 get -net ${route}/64
}

add_lowest_metric_cleanup()
{
	vnet_cleanup
}

atf_test_case "add_default_metric" "cleanup"
add_default_metric_head()
{
	atf_set descr 'Create a route and verify the default metric is set'
	atf_set require.user root
	atf_set require.progs jq
}

add_default_metric_body()
{
	local epair laddr route nhop1

	laddr="3fff::1"
	route="3fff:a::"
	nhop1="3fff::1"

	vnet_init
	epair=$(vnet_mkepair)

	atf_check -o ignore \
	    ifconfig ${epair}a inet6 ${laddr} up

	# Create a route without specifying its metric
	atf_check -o ignore \
	    route -6 add -net ${route}/64 -gateway ${nhop1}

	# Verify the route has the default metric of 1
	atf_check -o save:netstat \
	    netstat -rn6 --libxo json
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop1} 1 1)
	atf_check_equal "$output" "$nhop1"
}

add_default_metric_cleanup()
{
	vnet_cleanup
}

atf_test_case "delete_route_with_metric" "cleanup"
delete_route_with_metric_head()
{
	atf_set descr 'Create multiple routes to same dst and delete routes with specific metric'
	atf_set require.user root
	atf_set require.progs jq
}

delete_route_with_metric_body()
{
	local epair laddr route nhop1 nhop2

	laddr="3fff::1"
	route="3fff:a::"
	nhop1="3fff::1"
	nhop2="3fff::2"

	vnet_init
	epair=$(vnet_mkepair)

	atf_check -o ignore \
	    ifconfig ${epair}a inet6 ${laddr} up

	# Create two groups of ECMP routes with metric 2 and 3, and
	# another route with metric 4.
	atf_check -o ignore \
	    route -6 add -net ${route}/64 -gateway ${nhop1} -metric 3
	atf_check -o ignore \
	    route -6 add -net ${route}/64 -gateway ${nhop1} -weight 10 -metric 2
	atf_check -o ignore \
	    route -6 add -net ${route}/64 -gateway ${nhop2} -weight 10 -metric 2
	atf_check -o ignore \
	    route -6 add -net ${route}/64 -gateway ${nhop2} -metric 3
	atf_check -o ignore \
	    route -6 add -net ${route}/64 -gateway ${nhop2} -metric 4

	# Validate we have 5 routes
	atf_check -o save:netstat \
	    netstat -rn6 --libxo json
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop1} 1 3)
	atf_check_equal "$output" "$nhop1"
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop1} 10 2)
	atf_check_equal "$output" "$nhop1"
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop2} 10 2)
	atf_check_equal "$output" "$nhop2"
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop2} 1 3)
	atf_check_equal "$output" "$nhop2"
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop2} 1 4)
	atf_check_equal "$output" "$nhop2"

	# Delete one of the nexthops of them best ECMP route
	# Test that deleting a route by specifying gateway + metric works.
	atf_check -o ignore \
	    route -n6 delete -net ${route}/64 -gateway ${nhop2} -metric 2

	# Verify that nhop1 is the best route now
	atf_check -o match:".*gateway: ${nhop1}.*" \
	    route -n6 get -net ${route}/64

	# But other route with nhops2 should exists.
	atf_check -o save:netstat \
	    netstat -rn6 --libxo json
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop2} 1 3)
	atf_check_equal "$output" "$nhop2"
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop2} 1 4)
	atf_check_equal "$output" "$nhop2"

	# Delete routes with nhop1 as nexthop without specifying metric.
	# Test that deleting a route by gateway removes all routes with
	# that gateway, regardless of metric value.
	atf_check -o ignore \
	    route -n6 delete -net ${route}/64 -gateway ${nhop1}

	# Verify that nhop2 is the best route now
	atf_check -o match:".*gateway: ${nhop2}.*" \
	    route -n6 get -net ${route}/64

	# Delete routes with metric 3 without specifying their gateway.
	# Test that deleting a route by metric removes all routes with
	# that metric, regardless of gateway value.
	atf_check -o ignore \
	    route -n6 delete -net ${route}/64 -metric 3

	# Verify that nhop2 is still the best route with metric of 4
	atf_check -o match:".*gateway: ${nhop2}.*" \
	    route -n6 get -net ${route}/64
	output=$(cat netstat | jq_rtentry ${route}/64 | jq_nhop_filter ${nhop2} 1 4)
	atf_check_equal "$output" "$nhop2"
}

delete_route_with_metric_cleanup()
{
	vnet_cleanup
}


atf_init_test_cases()
{
	atf_add_test_case "add_lowest_metric"
	atf_add_test_case "add_default_metric"
	atf_add_test_case "delete_route_with_metric"
}
