FROM python:3.11-slim
WORKDIR /app
COPY discovery_beacon.py /app/
RUN chmod +x /app/discovery_beacon.py
CMD ["python3", "-u", "/app/discovery_beacon.py"]
