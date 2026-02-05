// config.js - 統一設定管理中心
const ALLOSAURUS_API = "https://allosaurus-api-878665537417.asia-east1.run.app/recognize";
const firebaseConfig = {
    apiKey: "AIzaSyAI5lx2XT3ysBhTREBW637s_AlWC49LYJQ",
    authDomain: "test-5dbba.firebaseapp.com",
    projectId: "test-5dbba"
};

// 1. 初始化 Firebase (確保不重複初始化)
if (!firebase.apps.length) firebase.initializeApp(firebaseConfig);
const db = firebase.firestore();
const auth = firebase.auth();

/**
 * 2. 統一獲取設定的方法
 * 供所有分頁呼叫，確保讀取到最新的本地快取
 */
const getGlobalSettings = () => ({
    lang: localStorage.getItem('userLang') || 'en-US',
    key: localStorage.getItem('gptApiKey') || '',
    course: localStorage.getItem('userCourse') || 'all'
});

/**
 * 3. 處理 Google 登入重定向結果 (僅記錄，不強制跳轉)
 */
auth.getRedirectResult().then((result) => {
    if (result && result.user) {
        console.log("登入成功:", result.user.email);
    }
}).catch((error) => {
    if (error.code === 'auth/account-exists-with-different-credential') {
        alert("此帳號已存在不同的驗證方式");
    }
});