# Use the official PostgreSQL image
FROM postgres:16-alpine

# Copy the initialization script to the magic directory
# Any .sql script here runs on container startup
COPY db_schema.sql /docker-entrypoint-initdb.d/