"""
clientTB.py

ThingsBoard REST client.

All interaction with ThingsBoard REST API is encapsulated in this class.

Google Python Style Guide compatible.
"""

from __future__ import annotations

import json
import os
from typing import Dict, Optional

import requests


class ThingsBoardClient:
    """REST client for ThingsBoard."""

    def __init__(
        self,
        url: str,
        username: str,
        password: str,
        connect_timeout: int = 10,
        upload_timeout: int = 300,
    ):
        """Initialize client.

        Args:
            url: ThingsBoard URL.
            username: Login.
            password: Password.
            connect_timeout: REST timeout.
            upload_timeout: File upload timeout.
        """
        self.base_url = url.rstrip("/")
        self.username = username
        self.password = password

        self.connect_timeout = connect_timeout
        self.upload_timeout = upload_timeout

        self.token = None

    @property
    def headers(self) -> Dict[str, str]:
        """Return authorization headers."""
        return {
            "X-Authorization": f"Bearer {self.token}",
            "Accept": "application/json",
        }

    @property
    def json_headers(self) -> Dict[str, str]:
        """Return JSON authorization headers."""
        headers = self.headers.copy()
        headers["Content-Type"] = "application/json"
        return headers

    # ------------------------------------------------------------------
    # Authentication
    # ------------------------------------------------------------------

    def login(self) -> bool:
        """Authenticate and receive JWT token."""

        url = f"{self.base_url}/api/auth/login"

        payload = {
            "username": self.username,
            "password": self.password,
        }

        try:

            response = requests.post(
                url,
                json=payload,
                timeout=self.connect_timeout,
            )

            response.raise_for_status()

            self.token = response.json()["token"]

            print("✅ JWT token received")

            return True

        except Exception as exc:
            print(f"❌ Login error: {exc}")
            return False

    # ------------------------------------------------------------------
    # Device
    # ------------------------------------------------------------------

    def get_device_by_name(
        self,
        device_name: str,
    ) -> Optional[str]:
        """Find device by name."""

        url = (
            f"{self.base_url}"
            f"/api/tenant/devices"
            f"?textSearch={device_name}"
            f"&pageSize=100&page=0"
        )

        try:

            response = requests.get(
                url,
                headers=self.headers,
                timeout=self.connect_timeout,
            )

            response.raise_for_status()

            devices = response.json().get("data", [])

            for device in devices:
                if device["name"] == device_name:
                    return device["id"]["id"]

        except Exception as exc:
            print(exc)

        return None

    def get_device_profile(
        self,
        device_id: str,
    ) -> Optional[str]:
        """Return device profile id."""

        url = f"{self.base_url}/api/device/{device_id}"

        try:

            response = requests.get(
                url,
                headers=self.headers,
                timeout=self.connect_timeout,
            )

            response.raise_for_status()

            profile = response.json()["deviceProfileId"]

            if isinstance(profile, dict):
                return profile["id"]

            return profile

        except Exception as exc:
            print(exc)

        return None

    # ------------------------------------------------------------------
    # OTA Packages
    # ------------------------------------------------------------------

    def find_existing_packages(
        self,
        title: str,
    ) -> Dict:
        """Return OTA packages with specified title."""

        url = (
            f"{self.base_url}"
            f"/api/otaPackages"
            f"?textSearch={title}"
            f"&pageSize=100&page=0"
        )

        result = {}

        try:

            response = requests.get(
                url,
                headers=self.headers,
                timeout=self.connect_timeout,
            )

            response.raise_for_status()

            for package in response.json().get("data", []):

                if package["title"] != title:
                    continue

                result[package["version"]] = {
                    "id": package["id"]["id"],
                    "version": package["version"],
                    "hasData": package.get("hasData", False),
                }

        except Exception as exc:
            print(exc)

        return result

    def create_package(
        self,
        fw_name: str,
        version: str,
        device_profile_id: Optional[str] = None,
        package_type: str = "FIRMWARE",
    ) -> Optional[str]:
        """Create OTA package."""

        payload = {
            "type": package_type,
            "title": fw_name,
            "version": version,
            "tag": fw_name,
        }

        if device_profile_id:
            payload["deviceProfileId"] = {
                "id": device_profile_id,
                "entityType": "DEVICE_PROFILE",
            }

        url = f"{self.base_url}/api/otaPackage"

        try:

            response = requests.post(
                url,
                headers=self.json_headers,
                json=payload,
                timeout=self.connect_timeout,
            )

            if response.status_code not in (200, 201):

                payload.pop("deviceProfileId", None)

                response = requests.post(
                    url,
                    headers=self.json_headers,
                    json=payload,
                    timeout=self.connect_timeout,
                )

            response.raise_for_status()

            package = response.json()["id"]

            if isinstance(package, dict):
                return package["id"]

            return package

        except Exception as exc:
            print(exc)

        return None

    def upload_package(
        self,
        package_id: str,
        firmware_path: str,
        sha256: str,
    ) -> bool:
        """Upload firmware into OTA package."""

        url = (
            f"{self.base_url}"
            f"/api/otaPackage/{package_id}"
            f"?checksum={sha256}"
            f"&checksumAlgorithm=SHA256"
        )

        try:

            with open(firmware_path, "rb") as fp:

                response = requests.post(
                    url,
                    headers=self.headers,
                    files={
                        "file": (
                            os.path.basename(firmware_path),
                            fp.read(),
                            "application/octet-stream",
                        )
                    },
                    timeout=self.upload_timeout,
                )

            response.raise_for_status()

            return True

        except Exception as exc:
            print(exc)

            return False

    # ------------------------------------------------------------------
    # Attributes
    # ------------------------------------------------------------------

    def update_attributes(
        self,
        device_id: str,
        attributes: Dict,
    ) -> bool:
        """Update SERVER_SCOPE attributes."""

        url = (
            f"{self.base_url}"
            f"/api/plugins/telemetry"
            f"/DEVICE/{device_id}"
            f"/attributes/SERVER_SCOPE"
        )

        try:

            response = requests.post(
                url,
                headers=self.json_headers,
                json=attributes,
                timeout=self.connect_timeout,
            )

            response.raise_for_status()

            return True

        except Exception as exc:
            print(exc)

        return False

    # ------------------------------------------------------------------
    # Device Profile
    # ------------------------------------------------------------------

    def get_package_info(
        self,
        package_id: str,
    ) -> Optional[Dict]:
        """Return OTA package info."""

        url = f"{self.base_url}/api/otaPackage/info/{package_id}"

        try:

            response = requests.get(
                url,
                headers=self.headers,
                timeout=self.connect_timeout,
            )

            response.raise_for_status()

            return response.json()

        except Exception as exc:
            print(exc)

        return None

    def get_device_profile_by_id(
        self,
        profile_id: str,
    ) -> Optional[Dict]:
        """Return full Device Profile."""

        url = f"{self.base_url}/api/deviceProfile/{profile_id}"

        try:

            response = requests.get(
                url,
                headers=self.headers,
                timeout=self.connect_timeout,
            )

            response.raise_for_status()

            return response.json()

        except Exception as exc:
            print(exc)

        return None

    def save_device_profile(
        self,
        profile: Dict,
    ) -> bool:
        """Save Device Profile."""

        profile.pop("createdTime", None)
        profile.pop("version", None)

        url = f"{self.base_url}/api/deviceProfile"

        try:

            response = requests.post(
                url,
                headers=self.json_headers,
                json=profile,
                timeout=self.connect_timeout,
            )

            response.raise_for_status()

            return True

        except Exception as exc:
            print(exc)

        return False

    def assign_package(
        self,
        package_id: str,
        device_profile_id: str,
    ) -> bool:
        """Assign OTA package through Device Profile."""

        package = self.get_package_info(package_id)

        if package is None:
            return False

        profile = self.get_device_profile_by_id(device_profile_id)

        if profile is None:
            return False

        reference = {
            "entityType": "OTA_PACKAGE",
            "id": package_id,
        }

        if package["type"] == "FIRMWARE":
            profile["firmwareId"] = reference
        else:
            profile["softwareId"] = reference

        return self.save_device_profile(profile)