## 📌 Pop-in Protocol 구현 현황 및 시나리오

### **구현된 기능 목록**

#### 1. **사용자 (User) 기능**
- ✅ RSSI 기반 부스 자동 탐색
- ✅ 최적 부스 선택 알고리즘 (신호 강도 + 가용성)
- ✅ 자동 연결 및 등록
- ✅ 부스 입장/퇴장 ('e' 키)
- ✅ 재입장 차단 (1인 1회 체험)
- ✅ 관리자 메시지 수신

#### 2. **관리자 (Admin) 기능**
- ✅ 부스 자동 브로드캐스트
- ✅ 커스텀 메시지 전송 ('m' 키)
- ✅ 부스 상태 확인 ('s' 키)
- ✅ 수동 공지 전송 ('a' 키)
- ✅ Quiet 모드 ('q' 키)
- ✅ 사용자 입/퇴장 관리
- ✅ 등록 이력 관리

#### 3. **프로토콜 기능**
- ✅ 양방향 통신 (일부 제한)
- ✅ 브로드캐스트/유니캐스트
- ✅ RSSI 신호 강도 측정
- ✅ 메시지 타입별 처리

---

### **실제 사용 시나리오**

#### **시나리오 1: 첫 방문 고객**
```
1. 고객(User 4) 팝업스토어 지역 진입
   → "Starting RSSI-based booth scanning..."

2. 주변 부스 자동 탐색 (2초간)
   → "Booth 1 detected! RSSI: -28 dBm"
   → "Booth 2 detected! RSSI: -45 dBm"

3. 최적 부스 자동 선택
   → "Selected Booth 1 as optimal choice" (가장 가까움)

4. 최적 부스의 정보 확인
 
   Connecting to Booth 1…

   [User] Received booth info from Booth 1
   
   === BOOTH INFORMATION ===
   Description: Booth 1 - Pop-up Store Experience
   Current users: 0/5
   Waiting users: 0

5. 입장 선택
 
   Would you like to experience this booth? (y/n): _
      'y' 또는 'Y':
         USER_RESPONSE (YES) 전송
         REGISTER_REQUEST 전송
         입장 진행
      'n' 또는 'N':
         USER_RESPONSE (NO) 전송
         스캔 모드로 복귀
         다른 부스 탐색 계속
   
   Sending registration request…
   
   Registration successful! Welcome to the booth!
   Your experience session has started.
   Press ‘e’ to exit the booth.

6. 체험 중 관리자 공지 수신
   → "[ADMIN BROADCAST] Welcome! 50% discount event!"

7. 체험 완료 후 퇴장 ('e' 키)
   → "Exited the booth."
```

#### **시나리오 2: 재방문 시도**
```
1. 동일 고객 다시 접근
   → 부스 자동 감지 및 연결

2. 재입장 시도
   → "Registration failed: Already experienced this booth"
   → "Each user can only experience a booth once."

3. 다른 부스로 자동 재탐색
   → 계속 스캔 모드 유지
```

#### **시나리오 3: 관리자 운영**
```
1. 부스 오픈 (Admin 1)
   → 자동 브로드캐스트 시작

2. 실시간 상태 확인 ('s' 키)
   === BOOTH STATUS ===
   Current Users: 3/5
   Waiting Queue: 2 users
   Total Registered: 7 users

3. 특별 이벤트 공지 ('m' 키)
   → "Enter message: Flash sale starts now!"
   → 모든 사용자에게 전송

4. 혼잡 시 조용한 모드 ('q' 키)
   → 자동 브로드캐스트 중지
   → 수동 관리 모드
```

#### **시나리오 4: 다중 부스 환경**
```
1. 3개 부스 동시 운영 (Booth 1, 2, 3)

2. User 5 진입 시 RSSI 측정:
   - Booth 1: -30 dBm (10m)
   - Booth 2: -50 dBm (30m)  
   - Booth 3: -70 dBm (50m)

3. 점수 계산:
   - Booth 1: -30 + 20 = -10점 (2/5 입장 가능)
   - Booth 2: -50 + 20 = -30점 (0/5 입장 가능)
   - Booth 3: -70 + 0 = -70점 (5/5 만석)

4. Booth 1 자동 선택 및 연결
```
