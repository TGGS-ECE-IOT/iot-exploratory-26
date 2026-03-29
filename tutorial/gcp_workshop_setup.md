# GCP Project Setup for IoT Workshop (Admin + Student Guide)

## Part 1: Overview

Use ONE shared GCP project owned by instructor.
Students join via IAM. No credit card needed.

Naming convention per team:
- team01_topic
- team01_dataset

---

## Part 2: Admin Setup

### Step 1: Create Project
Go to https://console.cloud.google.com
Create project: iot-workshop-2026

[Insert Screenshot]

### Step 2: Link Billing
Billing → Link project to billing account

[Insert Screenshot]

### Step 3: Enable APIs
Enable:
- Pub/Sub
- BigQuery
- Dataflow

[Insert Screenshot]

### Step 4: Add Students
IAM → Grant Access
Role: Editor

[Insert Screenshot]

### Step 5: Budget
Set budget (e.g. $10)

[Insert Screenshot]

---

## Part 3: Student Setup

### Step 1: Accept Invite
Open email → Open console

### Step 2: Select Project
Choose iot-workshop-2026

[Insert Screenshot]

### Step 3: Create Resources

Pub/Sub topic:
team01-topic

BigQuery dataset:
team01_dataset

Table:
sensor_data

---

## Rules
DO:
- Use team prefix

DO NOT:
- Create new project
- Add billing

---

## Troubleshooting
- Permission error → ask admin
- Billing error → admin check
