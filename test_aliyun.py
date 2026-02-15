#!/usr/bin/env python3
import os
import sys
import time
import random
import string
import hashlib
import hmac
import base64
import urllib.parse
import json

def parse_env(filepath):
    """Parse .env file"""
    config = {}
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if '=' in line:
                key, value = line.split('=', 1)
                key = key.strip()
                value = value.strip()
                # Remove quotes
                if (value.startswith('"') and value.endswith('"')) or (value.startswith("'") and value.endswith("'")):
                    value = value[1:-1]
                config[key] = value
    return config

def url_encode(s):
    """URL encode according to Aliyun's requirements"""
    # Aliyun expects percent-encoding with uppercase hex digits
    return urllib.parse.quote(s, safe='')

def generate_timestamp():
    """Generate ISO8601 timestamp in UTC"""
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

def generate_nonce(length=16):
    """Generate random string for SignatureNonce"""
    chars = string.ascii_letters + string.digits
    return ''.join(random.choice(chars) for _ in range(length))

def calculate_signature(access_key_secret, method, params):
    """Calculate HMAC-SHA1 signature for Aliyun API"""
    # Sort parameters by key
    sorted_keys = sorted(params.keys())
    sorted_params = '&'.join([f"{key}={url_encode(params[key])}" for key in sorted_keys])
    
    # String to sign
    string_to_sign = f"{method}&{url_encode('/')}&{url_encode(sorted_params)}"
    
    # HMAC-SHA1
    key = access_key_secret + '&'
    hashed = hmac.new(key.encode('utf-8'), string_to_sign.encode('utf-8'), hashlib.sha1)
    signature = base64.b64encode(hashed.digest()).decode('utf-8')
    
    return signature, sorted_params

def test_update_record():
    # Load config
    env_path = os.path.join(os.path.dirname(__file__), '.env')
    config = parse_env(env_path)
    
    access_key_id = config.get('AccessKeyID')
    access_key_secret = config.get('AccessKeySecret')
    record_id = config.get('RecordId')
    domain_name = config.get('domainName')
    
    if not all([access_key_id, access_key_secret, record_id, domain_name]):
        print("Missing required config in .env file")
        return
    
    # Determine RR (subdomain)
    # For root domain, RR should be "@"
    rr = "@"
    
    # Current public IP (from earlier)
    public_ip = "223.198.240.16"  # Replace with actual IP if needed
    
    print(f"AccessKeyId: {access_key_id[:10]}...")
    print(f"RecordId: {record_id}")
    print(f"Domain: {domain_name}")
    print(f"RR: {rr}")
    print(f"IP: {public_ip}")
    
    # API parameters
    params = {
        "Action": "UpdateDomainRecord",
        "RecordId": record_id,
        "RR": rr,
        "Type": "A",
        "Value": public_ip,
        "AccessKeyId": access_key_id,
        "Format": "JSON",
        "SignatureMethod": "HMAC-SHA1",
        "SignatureNonce": generate_nonce(),
        "SignatureVersion": "1.0",
        "Timestamp": generate_timestamp(),
        "Version": "2015-01-09"
    }
    
    # Calculate signature (GET method as used in ESP32 code)
    signature, sorted_params = calculate_signature(access_key_secret, "GET", params)
    
    # Add signature to params
    params["Signature"] = signature
    
    # Build URL for GET request
    encoded_params = '&'.join([f"{key}={url_encode(params[key])}" for key in sorted(params.keys())])
    url = f"https://alidns.aliyuncs.com/?{encoded_params}"
    
    print("\n=== GET Request ===")
    print(f"URL length: {len(url)}")
    print(f"URL: {url}")
    
    # Also prepare POST data
    post_data = {k: v for k, v in params.items() if k != "Signature"}
    post_data["Signature"] = signature
    
    print("\n=== POST Request ===")
    print("curl -X POST 'https://alidns.aliyuncs.com/' \\")
    for key in sorted(post_data.keys()):
        value = post_data[key]
        if key == "Signature":
            print(f" -d '{key}={value}'")
        else:
            print(f" -d '{key}={value}' \\")
    
    print("\n=== Debug Info ===")
    print(f"Timestamp: {params['Timestamp']}")
    print(f"SignatureNonce: {params['SignatureNonce']}")
    print(f"Signature: {signature}")
    
    # Ask user if they want to execute the request
    print("\nExecute GET request with curl? (y/n): ", end="")
    choice = input().strip().lower()
    
    if choice == 'y':
        # Execute GET request
        import subprocess
        cmd = ["curl", "-s", "-H", "User-Agent: ESP32-Aliyun-DDNS/1.0", url]
        print(f"\nExecuting: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True)
        print(f"HTTP Response: {result.stdout}")
        if result.stderr:
            print(f"Error: {result.stderr}")

if __name__ == "__main__":
    test_update_record()