FROM python:3.11-alpine

WORKDIR /app

ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

COPY . .

# Expose port for backend & dashboard
EXPOSE 8000

CMD ["python", "server.py"]

