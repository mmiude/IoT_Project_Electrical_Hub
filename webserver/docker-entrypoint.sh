#!/bin/sh
set -e

KEY_DIR="/app/keys"
PRIVATE_KEY="$KEY_DIR/private_key.pem"
PUBLIC_KEY="$KEY_DIR/public_key.pem"

# Ensure the keys directory exists
mkdir -p "$KEY_DIR"

# Generate RSA keypair if missing
if [ ! -f "$PRIVATE_KEY" ] || [ ! -f "$PUBLIC_KEY" ]; then
    echo "Keys not found in $KEY_DIR. Generating new RSA keypair..."
    
    # Generate 2048-bit private key
    openssl genrsa -out "$PRIVATE_KEY" 2048
    
    # Extract public key
    openssl rsa -in "$PRIVATE_KEY" -pubout -out "$PUBLIC_KEY"
    
    echo "Keys generated successfully."
fi

# Execute the container's CMD
exec "$@"