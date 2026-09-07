#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define MAX_DATA 512
#define MAX_ROWS 100

struct Address {
	int id;
	int set;
	char name[MAX_DATA];
	char email[MAX_DATA];
};

struct Database {
	struct Address rows[MAX_ROWS];
};

struct Connection {
	FILE *file;
	struct Database *db;
};

void die(const char *message)
{
	if (errno) {
		perror(message);
	} else {
		printf("Error: %s\n", message);
	}
}

void printAddress(struct Address *addr)
{
	printf("%d %s %s\n",
		addr->id, addr->name, addr->email);
}

void loadDB(struct Connection *conn)
{
	int rc = fread(conn->db, sizeof(struct Database),
		1, conn->file);

	if (rc != 1) {
		die("Failed to load DB");
	}
}

struct Connection *openDB(const char *filename, char mode)
{
	struct Connection *conn = malloc(sizeof(struct Connection));

	if (!conn) {
		die("Memory error");
	}

	if (mode == 'c') {
		conn->file = fopen(filename, "c");
	} else {
		conn->file = fopen(filename, "r+");;

		if (conn->file) {
			loadDB(conn);
		}
	}

	if (!conn->file) {
		die("Failed to load DB");
	}

	return conn;
}

void closeDB(struct Connection *conn)
{
	if (conn) {
		if (conn->file) fclose(conn->file);
		if (conn->db) free(conn->db);
		free(conn);
	}
}

void writeDB(struct Connection *conn)
{
	rewind(conn->file);

	int rc = fwrite(conn->db, sizeof(struct Database),
		1, conn->file);
	if(rc != 1) die("Failed to write to DB");

	rc = fflush(conn->file);
	if(rc == -1) die("Cannot flush DB");
}

void createDB(struct Connection *conn)
{
	int i = 0;

	for (i = 0; i < MAX_ROWS; i++) {
		struct Address addr = {.id = i, .set = 0};

		conn->db->rows[i] = addr;
	}
}

void setDB(struct Connection *conn,
	int id, const char *name, const char *email)
{
	struct Address *addr = &conn->db->rows[id];
	if(addr->set) die("Already set");

	addr->set = 1;

	char *res = strncpy(addr->name, name, MAX_DATA);
	if(!res) die("Name copy failed");

	res = strncpy(addr->email, email, MAX_DATA);
	if(!res) die("Email copy failed");
}

void getDB(struct Connection *conn, int id)
{
	struct Address *addr = &conn->db->rows[id];

	if (addr->set) {
		printAddress(addr);
	} else {
		die("ID is not set");
	}
}

void delDB(struct Connection *conn, int id)
{
	struct Address addr = {.id = id, .set = 0};
	conn->db->rows[id] = addr;
}

void listDB(struct Connection *conn)
{
	int i = 0;
	struct Database *db = conn->db;

	for (i = 0; i < MAX_ROWS; i++) {
		struct Address *curr = &db->rows[i];

		if (curr->set) {
			printAddress(curr);
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		die("USAGE ex17 {dbfile} {action} {params}");
	}

	char *filename = argv[1];
	char action = argv[2][0];

	struct Connection *conn = openDB(filename, action);

	int id = 0;

	if (argc > 3) {
		id = atoi(argv[3]);
	}

	switch(action){
		case 'c':
			createDB(conn);
			writeDB(conn);
			break;
		case 'g':
			if(argc != 4) die("Need an id to get");
			getDB(conn, id);
			break;
		case 's':
			if(argc != 6) die("Need id, name, email");
			setDB(conn, id, argv[4], argv[5]);
			writeDB(conn);
			break;
		case 'd':
			if(argc != 4) die("Need id to delete");
			delDB(conn, id);
			writeDB(conn);
			break;
		case 'l':
			listDB(conn);
			break;
		default:
			die("Invalid action");

	}

	closeDB(conn);

	return 0;
}
