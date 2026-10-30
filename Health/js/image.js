// 壓縮照片。Firestore 單一文件上限 1MB，所以超過就再縮小
const MAX_BYTES = 800_000;

export async function compressImage(file) {
  const bitmap = await createImageBitmap(file);
  let maxSide = 1280;
  let dataUrl = '';
  do {
    const scale = Math.min(1, maxSide / Math.max(bitmap.width, bitmap.height));
    const canvas = document.createElement('canvas');
    canvas.width = Math.round(bitmap.width * scale);
    canvas.height = Math.round(bitmap.height * scale);
    canvas.getContext('2d').drawImage(bitmap, 0, 0, canvas.width, canvas.height);
    dataUrl = canvas.toDataURL('image/jpeg', 0.72);
    maxSide = Math.round(maxSide * 0.8);
  } while (dataUrl.length > MAX_BYTES && maxSide > 320);
  bitmap.close();
  return dataUrl;
}
