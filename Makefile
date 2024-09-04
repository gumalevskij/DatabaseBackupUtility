all: build

BINARY_NAME=pg_backup_restore
DB_NAME=20240903225934_FULL

build:
	gcc -o $(BINARY_NAME) pg_backup_restore.c

run: build
	./$(BINARY_NAME) ~/repository full_backup ~/pgdata_test

restore: build
	./$(BINARY_NAME) ~/repository restore ~/pgdata3 $(DB_NAME)

clean:
	rm $(BINARY_NAME)
