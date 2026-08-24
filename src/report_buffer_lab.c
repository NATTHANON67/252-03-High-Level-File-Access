#include "report_buffer_lab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim_newline(char *line) {
	size_t length;

	length = strlen(line);
	if (length > 0 && line[length - 1] == '\n') {
		line[length - 1] = '\0';
	}
}

int load_orders(FILE *in, struct order_record records[], size_t capacity, struct lab_stats *stats) {
	char line[LAB_MAX_LINE_LEN];
	size_t count;

	if (in == NULL || records == NULL || stats == NULL) {
		return -1;
	}

	memset(stats, 0, sizeof(*stats));
	count = 0;

	while (fgets(line, sizeof(line), in) != NULL) {
		char name[LAB_MAX_NAME_LEN];
		char category[LAB_MAX_CATEGORY_LEN];
		char extra;
		int quantity;
		int unit_price;
		int scanned;
		int total_price;
		size_t name_length;

		stats->input_reads++;
		trim_newline(line);

		if (line[0] == '\0') {
			continue;
		}

		if (count == capacity) {
			fprintf(stderr, "too many records\n");
			return -1;
		}

		/*
		 * Width-limited fields:
		 *   %31[^|] -> at most 31 characters for name
		 *   %15[^|] -> at most 15 characters for category
		 *
		 * The final %c detects extra data after the fourth field.
		 * A valid record therefore produces exactly 4 conversions.
		 */
		scanned = sscanf(line,
				 "%31[^|]|%d|%d|%15[^|]%c",
				 name,
				 &quantity,
				 &unit_price,
				 category,
				 &extra);

		if (scanned != 4 || quantity < 0 || unit_price < 0) {
			continue;
		}

		snprintf(records[count].name,
			 sizeof(records[count].name),
			 "%s",
			 name);

		records[count].quantity = quantity;
		records[count].unit_price = unit_price;

		snprintf(records[count].category,
			 sizeof(records[count].category),
			 "%s",
			 category);

		total_price = quantity * unit_price;
		records[count].total_price = total_price;

		stats->grand_total += total_price;

		if (count == 0 || total_price > stats->max_total) {
			stats->max_total = total_price;
		}

		name_length = strlen(name);
		if (name_length > stats->longest_name) {
			stats->longest_name = name_length;
		}

		count++;
	}

	stats->records_loaded = count;
	return 0;
}

int build_report(const struct order_record records[], size_t count,
		 const struct lab_stats *stats, char *out, size_t out_size) {
	size_t offset;
	size_t i;
	int written;

	if (records == NULL || stats == NULL || out == NULL || out_size == 0) {
		return -1;
	}

	offset = 0;

	/*
	 * Header
	 */
	written = snprintf(out + offset,
			   out_size - offset,
			   "%-*s  %5s  %10s  %-*s  %5s\n",
			   (int)stats->longest_name,
			   "Name",
			   "Qty",
			   "Unit Price",
			   LAB_MAX_CATEGORY_LEN - 1,
			   "Category",
			   "Total");

	if (written < 0 || (size_t)written >= out_size - offset) {
		return -1;
	}
	offset += (size_t)written;

	/*
	 * Rows
	 */
	for (i = 0; i < count; i++) {
		written = snprintf(out + offset,
				   out_size - offset,
				   "%-*s  %5d  %10d  %-*s  %5d\n",
				   (int)stats->longest_name,
				   records[i].name,
				   records[i].quantity,
				   records[i].unit_price,
				   LAB_MAX_CATEGORY_LEN - 1,
				   records[i].category,
				   records[i].total_price);

		if (written < 0 || (size_t)written >= out_size - offset) {
			return -1;
		}
		offset += (size_t)written;
	}

	/*
	 * Summary
	 */
	written = snprintf(out + offset,
			   out_size - offset,
			   "Grand total: %d\n"
			   "Max total: %d\n"
			   "Longest name: %zu\n",
			   stats->grand_total,
			   stats->max_total,
			   stats->longest_name);

	if (written < 0 || (size_t)written >= out_size - offset) {
		return -1;
	}

	return 0;
}

int main(int argc, char **argv) {
	FILE *in;
	struct order_record records[LAB_MAX_RECORDS];
	struct lab_stats stats;
	char report[LAB_REPORT_CAPACITY];
	size_t report_length;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <orders-file>\n", argv[0]);
		return 1;
	}

	in = fopen(argv[1], "r");
	if (in == NULL) {
		perror("fopen");
		return 1;
	}

	if (load_orders(in, records, LAB_MAX_RECORDS, &stats) != 0) {
		fclose(in);
		return 1;
	}

	if (fclose(in) != 0) {
		perror("fclose");
		return 1;
	}

	stats.output_writes = 1;

	if (build_report(records, stats.records_loaded, &stats,
			 report, sizeof(report)) != 0) {
		fprintf(stderr, "failed to build report\n");
		return 1;
	}

	report_length = strlen(report);

	if (fwrite(report, 1, report_length, stdout) != report_length) {
		perror("fwrite");
		return 1;
	}

	return 0;
}