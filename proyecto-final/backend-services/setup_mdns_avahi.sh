#!/usr/bin/env bash
# ==============================================================================
# Script de Publicación y Resolución mDNS para el Sistema IoT Geofence
# Configura Avahi Daemon en Arch Linux / Linux host para publicar:
#   http://geofence.local -> Puerto 80 (Frontend React/Nginx)
#   mqtt://geofence.local -> Puerto 1883 (Broker Mosquitto)
# ==============================================================================

set -e

echo "============================================================"
echo "    IOT GEOFENCE // CONFIGURADOR mDNS (AVAHI DAEMON)"
echo "============================================================"

# Verificar si se ejecuta como root / sudo para escribir en /etc/avahi/services/
if [ "$EUID" -ne 0 ]; then
  echo "[-] Este script requiere permisos de superusuario (sudo) para instalar el servicio en /etc/avahi/services/."
  echo "    Por favor ejecuta: sudo ./setup_mdns_avahi.sh"
  exit 1
fi

AVAHI_SERVICE_DIR="/etc/avahi/services"
SERVICE_FILE="$AVAHI_SERVICE_DIR/geofence.service"

echo "[+] Verificando instalación de avahi..."
if ! command -v avahi-daemon &> /dev/null; then
  echo "[-] avahi-daemon no está instalado. Instalando con pacman..."
  pacman -S --noconfirm avahi nss-mdns
fi

echo "[+] Habilitando y asegurando avahi-daemon..."
systemctl enable avahi-daemon.service || true
systemctl start avahi-daemon.service || true

echo "[+] Generando configuración de servicio Avahi en: $SERVICE_FILE"

cat <<EOF > "$SERVICE_FILE"
<?xml version="1.0" standalone='no'?><!--*-nxml-*-->
<!DOCTYPE service-group SYSTEM "avahi-service.dtd">
<service-group>
  <name replace-wildcards="yes">GEOFENCE Command Center (%h)</name>
  <service>
    <type>_http._tcp</type>
    <port>80</port>
    <host-name>geofence.local</host-name>
  </service>
  <service>
    <type>_mqtt._tcp</type>
    <port>1883</port>
    <host-name>geofence.local</host-name>
  </service>
</service-group>
EOF

echo "[+] Reiniciando avahi-daemon para aplicar geofence.local..."
systemctl restart avahi-daemon.service

echo "[+] ¡Configuración completada con éxito!"
echo "------------------------------------------------------------"
echo "Ahora puedes acceder desde cualquier dispositivo en tu red local a:"
echo "   🟢 Dashboard UI : http://geofence.local"
echo "   🟢 MQTT Broker  : geofence.local:1883"
echo "============================================================"
