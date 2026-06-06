from keygen import KEY_DIR
from cryptography.hazmat.primitives import serialization

def load_private_key():
    with open(f"{KEY_DIR}/private_key.pem", "rb") as f:
        private_key = serialization.load_pem_private_key(f.read(), None)
        return private_key
    
def load_public_key():
    with open(f"{KEY_DIR}/public_key.pem", "rb") as f:
        public_key = serialization.load_pem_public_key(f.read())
        return public_key
    