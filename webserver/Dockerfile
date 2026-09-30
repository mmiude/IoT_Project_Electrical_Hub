FROM node:20-alpine

# Install openssl (required for key generation on alpine)
RUN apk add --no-cache openssl

WORKDIR /app

COPY package*.json ./
RUN npm install

COPY . .

# Copy entrypoint script and make it executable
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

EXPOSE 3000 8080

ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["npm", "start"]