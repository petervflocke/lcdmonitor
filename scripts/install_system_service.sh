#!/usr/bin/env bash
set -euo pipefail

log() {
  printf '[install] %s\n' "$*"
}

require_root() {
  if [[ "${EUID}" -ne 0 ]]; then
    log "This installer must run as root (try: sudo make service-system-install)."
    exit 1
  fi
}

SERVICE_USER=${SERVICE_USER:-lcdmon}
SERVICE_GROUP=${SERVICE_GROUP:-dialout}
INSTALL_ROOT=${INSTALL_ROOT:-/opt/lcdmonitor}
CONFIG_PATH=${CONFIG_PATH:-/etc/lcdmonitor/config.yaml}
ENV_FILE=${ENV_FILE:-/etc/default/lcdmonitor}
UNIT_NAME=${UNIT_NAME:-lcdmonitor.service}
SYSTEMD_DIR=${SYSTEMD_DIR:-/etc/systemd/system}
ENABLE_SERVICE=${ENABLE_SERVICE:-1}
COPY_REPO=${COPY_REPO:-1}
REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

require_root

log "Using service user=${SERVICE_USER} group=${SERVICE_GROUP}"
if ! id -u "${SERVICE_USER}" >/dev/null 2>&1; then
  log "WARNING: service user ${SERVICE_USER} is missing. Create it (e.g., sudo useradd -r -s /usr/sbin/nologin -G ${SERVICE_GROUP} ${SERVICE_USER}) before enabling the service."
fi
if ! getent group "${SERVICE_GROUP}" >/dev/null 2>&1; then
  log "WARNING: service group ${SERVICE_GROUP} is missing. Create it or override SERVICE_GROUP before continuing."
fi

log "Repository source: ${REPO_ROOT}"
log "Deployment target: ${INSTALL_ROOT} (server package expected under ${INSTALL_ROOT}/server)"
log "Expecting virtualenv at ${INSTALL_ROOT}/.venv (override via ENV_FILE if different)"

log "Preparing directories"
install -d -m 0755 "${INSTALL_ROOT}"
install -d -m 0755 "${INSTALL_ROOT}/server"
install -d -m 0755 "$(dirname "${CONFIG_PATH}")"
install -d -m 0755 "$(dirname "${ENV_FILE}")"

if [[ "${COPY_REPO}" -eq 1 ]]; then
  log "Syncing repository from ${REPO_ROOT} to ${INSTALL_ROOT}"
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --delete \
      --exclude='.git' \
      --exclude='.venv' \
      --exclude='server/.venv' \
      --exclude='__pycache__' \
      "${REPO_ROOT}/" "${INSTALL_ROOT}/"
  else
    log "rsync not found; using tar fallback (no deletion)."
    tar -C "${REPO_ROOT}" \
      --exclude='.git' \
      --exclude='.venv' \
      --exclude='server/.venv' \
      --exclude='__pycache__' \
      -cf - . | tar -C "${INSTALL_ROOT}" -xf -
  fi
else
  log "COPY_REPO=${COPY_REPO}; skipping repository sync"
fi

log "Seeding config & env files"
if [[ ! -e "${CONFIG_PATH}" ]]; then
  install -m 0640 "${REPO_ROOT}/server/config.example.yaml" "${CONFIG_PATH}"
  log "Created config from example at ${CONFIG_PATH}"
else
  log "Config already exists at ${CONFIG_PATH}; leaving untouched"
fi

if [[ ! -e "${ENV_FILE}" ]]; then
  cat <<EOF_ENV >"${ENV_FILE}"
LCDMONITOR_VENV=${INSTALL_ROOT}/.venv
LCDMONITOR_CONFIG=${CONFIG_PATH}
EOF_ENV
  chmod 0640 "${ENV_FILE}"
  log "Created ${ENV_FILE}; update LCDMONITOR_VENV after provisioning the virtualenv"
else
  log "Env file already exists at ${ENV_FILE}; leaving untouched"
fi

if id -u "${SERVICE_USER}" >/dev/null 2>&1 && getent group "${SERVICE_GROUP}" >/dev/null 2>&1; then
  if [[ -e "${CONFIG_PATH}" ]]; then
    chown "${SERVICE_USER}:${SERVICE_GROUP}" "${CONFIG_PATH}"
    log "Updated ownership for ${CONFIG_PATH} to ${SERVICE_USER}:${SERVICE_GROUP}"
  fi
  service_home=$(getent passwd "${SERVICE_USER}" | cut -d: -f6)
  if [[ -n "${service_home}" ]]; then
    install -d -m 0750 -o "${SERVICE_USER}" -g "${SERVICE_GROUP}" "${service_home}/.cache/pip"
    log "Ensured pip cache directory at ${service_home}/.cache/pip"
  else
    log "WARNING: could not determine home directory for ${SERVICE_USER}; pip cache not pre-created"
  fi
else
  log "Skipped config ownership update (user or group missing); adjust permissions manually if needed"
fi

tmp_unit=$(mktemp)
trap 'rm -f "${tmp_unit}"' EXIT

sed \
  -e "s|^User=.*|User=${SERVICE_USER}|" \
  -e "s|^Group=.*|Group=${SERVICE_GROUP}|" \
  -e "s|^WorkingDirectory=.*|WorkingDirectory=${INSTALL_ROOT}/server|" \
  -e "s|^EnvironmentFile=.*|EnvironmentFile=${ENV_FILE}|" \
  "${REPO_ROOT}/infra/systemd/lcdmonitor.system.service" >"${tmp_unit}"

install -m 0644 "${tmp_unit}" "${SYSTEMD_DIR}/${UNIT_NAME}"
log "Installed systemd unit at ${SYSTEMD_DIR}/${UNIT_NAME}"

if id -u "${SERVICE_USER}" >/dev/null 2>&1 && getent group "${SERVICE_GROUP}" >/dev/null 2>&1; then
  chown -R "${SERVICE_USER}:${SERVICE_GROUP}" "${INSTALL_ROOT}"
  log "Updated ownership for ${INSTALL_ROOT} to ${SERVICE_USER}:${SERVICE_GROUP}"
else
  log "Skipped ownership update (user or group missing); create them and rerun if needed"
fi

systemctl daemon-reload
if [[ "${ENABLE_SERVICE}" -eq 1 ]]; then
  systemctl enable --now "${UNIT_NAME}"
  log "Enabled and started ${UNIT_NAME}"
else
  log "ENABLE_SERVICE=${ENABLE_SERVICE}; not enabling or starting ${UNIT_NAME}"
fi

log "Done. Ensure code is deployed under ${INSTALL_ROOT}/server and virtualenv matches ${ENV_FILE}."
