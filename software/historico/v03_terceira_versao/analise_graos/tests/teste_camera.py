import cv2

cap = cv2.VideoCapture(0, cv2.CAP_DSHOW)  # índice 1 = câmera USB

# Força uma resolução comum
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

if not cap.isOpened():
    print("❌ Erro: câmera não foi aberta.")
else:
    print("✅ Câmera aberta. Pressione ESC para sair.")
    while True:
        ret, frame = cap.read()
        if not ret:
            print("⚠️ Frame não capturado.")
            break
        cv2.imshow("Câmera USB", frame)
        if cv2.waitKey(1) == 27:  # ESC
            break

cap.release()
cv2.destroyAllWindows()
