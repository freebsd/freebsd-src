# program to scan peer_summary file and produce summary of daily summaries
#
{
	if (NF < 8 || $1 == "ident")
		continue
	i = n
	for (j = 0; j < n; j++) {
		if ($1 == peer_ident[j])
			i = j
	}
	if (i == n) {
		peer_ident[i] = $1
		n++
	}
	peer_count[i]++
	if (($7 - $6 / 2) < 400) {
		peer_count[i]++
		peer_mean[i] += $3
		peer_var[i] += $4 * $4
		if ($5 > peer_max[i])
			peer_max[i] = $5
		if ($5 > 1)
			peer_1[i]++
		if ($5 > 5)
			peer_2[i]++
		if ($5 > 10)
			peer_3[i]++
		if ($5 > 50)
			peer_4[i]++
	}
} END {
	printf "       host     cnt     mean       rms       max   >1  >5 >10 >50\n"
	printf "=================================================================\n"
	for (i = 0; i < n; i++) {
		if (peer_count[i] <= 0)
			continue
		peer_mean[i] /= peer_count[i]
		peer_var[i] = sqrt(peer_var[i] / peer_count[i])
		printf "%15s%4d%10.3f%10.3f%10.3f%4d%4d%4d%4d\n", peer_ident[i], peer_count[i], peer_mean[i], peer_var[i], peer_max[i], peer_1[i], peer_2[i], peer_3[i], peer_4[i]
	}
}
