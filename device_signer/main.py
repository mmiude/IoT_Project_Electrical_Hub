from sign import sign
from verify import verify
import psycopg2

def get_connection():
    try:
        return psycopg2.connect(
            database="spikestriker",
            user="admin",
            password="admin",
            host="127.0.0.1",
            port=5432,
        )
    except:
        return False
    

def main():
    conn = get_connection()
    if not conn:
        print("Error connecting to db")
        return
    
    curr = conn.cursor()

    device_id = input("Enter device_id: ")
    device_id_b = bytes(device_id, "utf-8")
    #TODO: Check if device_id is valid

    signature = sign(device_id_b)
    curr.execute("INSERT INTO hub (id, signature) VALUES (%s, %s)", (device_id, signature))
    conn.commit()
    #TODO: Save signature and device_id to db
    print(f"Signed {device_id}. Signature: \n {signature}")

    # print("Verifying...")
    # if verify(signature, device_id_b):
    #     print("Signature valid :)")
    # else:
    #     print("Signature invalid :(")

if __name__ == "__main__":
    main()