#!/usr/bin/env python3
"""
Servicio de descubrimiento por beacon UDP para el Gateway Geofence (ESP32).
Emite periódicamente un paquete UDP broadcast con la IP del broker MQTT.
Además, escucha solicitudes activas ('GEOFENCE_DISCOVER') y responde de inmediato.
Diseñado para funcionar en redes Hotspot móvil donde mDNS/multicast no opera.
"""
import socket
import time
import threading
import os

BEACON_PORT = int(os.environ.get("DISCOVERY_BEACON_PORT", 19883))
BROKER_PORT = int(os.environ.get("MQTT_BROKER_PORT", 1883))
BEACON_INTERVAL = float(os.environ.get("DISCOVERY_INTERVAL_SEC", 3.0))
MAGIC_PREFIX = "GEOFENCE_MQTT"


def get_local_ip():
    """Obtiene la IP local principal conectándose a una IP pública dummy (no envía tráfico)."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


def beacon_sender(sock):
    """Envía beacons UDP broadcast periódicamente."""
    while True:
        try:
            ip = get_local_ip()
            message = f"{MAGIC_PREFIX}:{ip}:{BROKER_PORT}"
            sock.sendto(message.encode("utf-8"), ("255.255.255.255", BEACON_PORT))
        except Exception as e:
            print(f"[BEACON] Error enviando beacon: {e}")
        time.sleep(BEACON_INTERVAL)


def discovery_listener(sock):
    """Escucha solicitudes activas de descubrimiento desde el gateway y responde inmediatamente."""
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            text = data.decode("utf-8", errors="ignore").strip()
            if text == "GEOFENCE_DISCOVER":
                ip = get_local_ip()
                message = f"{MAGIC_PREFIX}:{ip}:{BROKER_PORT}"
                print(f"[BEACON] Solicitud activa recibida de {addr[0]}:{addr[1]}. Respondiendo: {message}")
                # Responder directamente a quien preguntó
                sock.sendto(message.encode("utf-8"), addr)
                # Y enviar un broadcast general adicional por si acaso
                sock.sendto(message.encode("utf-8"), ("255.255.255.255", BEACON_PORT))
        except Exception as e:
            print(f"[BEACON] Error escuchando solicitudes: {e}")


def main():
    print(f"=== Iniciando Servicio de Descubrimiento UDP Geofence ===")
    print(f"Puerto Beacon: {BEACON_PORT} | Puerto MQTT informado: {BROKER_PORT} | Intervalo: {BEACON_INTERVAL}s")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        sock.bind(("", BEACON_PORT))
    except Exception as e:
        print(f"[BEACON] Error haciendo bind al puerto {BEACON_PORT}: {e}")
        return

    # Iniciar hilo emisor periódico
    sender_thread = threading.Thread(target=beacon_sender, args=(sock,), daemon=True)
    sender_thread.start()

    # Hilo principal escucha solicitudes activas
    discovery_listener(sock)


if __name__ == "__main__":
    main()
