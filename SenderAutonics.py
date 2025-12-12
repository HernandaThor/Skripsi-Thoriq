import paho.mqtt.client as mqtt
import csv
import os
import json
from datetime import datetime, timedelta, timezone

# Nama file CSV
csv_file = "namafileyangingindibuat.csv"

WIB = timezone(timedelta(hours=7))
def on_connect(client, userdata, flags, rc):
    print("Terhubung ke MQTT dengan kode:", str(rc))
    if rc == 0:
        client.subscribe("topikyangdigunakan")
    else:
        print("Gagal connect, rc=", rc)

# Callback saat pesan diterima
def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    print("\nData diterima:", payload)

    try:
        data = json.loads(payload)

        # Timestamp lokal (UTC+7)
        timestamp = datetime.now(WIB).strftime("%Y-%m-%d %H:%M:%S")

        # Ambil data dari payload JSON
        pzem1 = data.get("pzem1", {})
        autonics = data.get("autonics", {})

        # Pisahkan nilai TM2 dan TM4 dari objek autonics
        tm2 = {
            "ch1": autonics.get("tm2_ch1", 0),
            "ch2": autonics.get("tm2_ch2", 0)
        }
        tm4 = {
            "ch1": autonics.get("tm4_ch1", 0),
            "ch2": autonics.get("tm4_ch2", 0),
            "ch3": autonics.get("tm4_ch3", 0),
            "ch4": autonics.get("tm4_ch4", 0)
        }

        # Siapkan baris data untuk CSV
        row = [
            timestamp,
            pzem1.get("voltage", 0),
            pzem1.get("current", 0),
            pzem1.get("power", 0),
            pzem1.get("energy", 0),
            data.get("lm35_t1", 0),
            data.get("lm35_t2", 0),
            data.get("lm35_t3", 0),
            data.get("dht_temp", 0),
            data.get("dht_hum", 0),
            tm2["ch1"],
            tm2["ch2"],
            tm4["ch1"],
            tm4["ch2"],
            tm4["ch3"],
            tm4["ch4"]
        ]

        # Tulis ke file CSV
        file_exists = os.path.isfile(csv_file)
        with open(csv_file, mode='a', newline='') as file:
            writer = csv.writer(file)
            # Tulis header jika file baru
            if not file_exists:
                writer.writerow([
                    "Timestamp",
                    "V1", "I1", "P1", "E1",
                    "lm35_t1", "lm35_t2", "lm35_t3",
                    "dht_temp", "dht_hum",
                    "Sisi Kiri", "Sisi Kiri",
                    "Sisi Atas", "Sisi Atas", "Sisi Kanan", "Sisi Kanan"
                ])
            writer.writerow(row)

        # Tampilkan data di terminal
        print(f"\n[{timestamp}]")
        print(f"PZEM1 => V: {pzem1.get('voltage', 0)} V | I: {pzem1.get('current', 0)} A | P: {pzem1.get('power', 0)} W | E: {pzem1.get('energy', 0)} Wh")
        print(f"DHT => Suhu: {data.get('dht_temp', 0)} °C | Kelembapan: {data.get('dht_hum', 0)} %")
        print("Sensor LM35:")
        print(f"  T1: {data.get('lm35_t1', 0)} °C | T2: {data.get('lm35_t2', 0)} °C | T3: {data.get('lm35_t3', 0)} °C")
        print("Autonics TM2:")
        print(f"  CH1: {tm2['ch1']} °C | CH2: {tm2['ch2']} °C")
        print("Autonics TM4:")
        print(f"  CH1: {tm4['ch1']} °C | CH2: {tm4['ch2']} °C | CH3: {tm4['ch3']} °C | CH4: {tm4['ch4']} °C")

    except Exception as e:
        print("Error parsing:", e)

# Konfigurasi MQTT client
client = mqtt.Client()
client.username_pw_set("usnmqtt", "passwordmqtt(jikaada)")
client.on_connect = on_connect
client.on_message = on_message

# Hubungkan ke broker MQTT
client.connect("servermqtt", portmqtt, 60)

print("Menunggu data dari ESP32... Tekan Ctrl+C untuk berhenti.")
try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\nBerhenti.")
