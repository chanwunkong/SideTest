// healthStore：整個 App 唯一能讀寫資料的地方。未登入存在本機，登入後存 Firestore
import { firebaseConfig } from './firebase-config.js';
import { dateKey, goalFor } from './logic.js';
import { normalizeEvent, registry } from './registry.js';

const COLLECTIONS = ['events', 'goals', 'attachments', 'profile'];

class LocalBackend {
  #dbPromise = new Promise((resolve, reject) => {
    const request = indexedDB.open('health', 1);
    request.onupgradeneeded = () => request.result.createObjectStore('docs', { keyPath: 'key' });
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });

  async #run(mode, fn) {
    const db = await this.#dbPromise;
    return new Promise((resolve, reject) => {
      const tx = db.transaction('docs', mode);
      const request = fn(tx.objectStore('docs'));
      tx.oncomplete = () => resolve(request.result);
      tx.onerror = () => reject(tx.error);
    });
  }

  async list(col) {
    const range = IDBKeyRange.bound(`${col}/`, `${col}/￿`);
    const rows = await this.#run('readonly', (s) => s.getAll(range));
    return rows.map((r) => r.doc);
  }
  get(col, id) {
    return this.#run('readonly', (s) => s.get(`${col}/${id}`)).then((r) => r?.doc ?? null);
  }
  put(col, id, doc) {
    return this.#run('readwrite', (s) => s.put({ key: `${col}/${id}`, doc }));
  }
  delete(col, id) {
    return this.#run('readwrite', (s) => s.delete(`${col}/${id}`));
  }
}

class FirestoreBackend {
  constructor(db, uid) {
    this.root = db.collection('users').doc(uid);
  }
  async list(col) {
    const snap = await this.root.collection(col).get();
    return snap.docs.map((d) => d.data());
  }
  async get(col, id) {
    const snap = await this.root.collection(col).doc(id).get();
    return snap.exists ? snap.data() : null;
  }
  put(col, id, doc) {
    return this.root.collection(col).doc(id).set(doc);
  }
  delete(col, id) {
    return this.root.collection(col).doc(id).delete();
  }
}

class HealthStore extends EventTarget {
  user = null;
  events = new Map();
  goals = [];
  profile = {};
  #local = new LocalBackend();
  #backend = this.#local;
  #attachments = new Map();
  #db = null;
  #auth = null;

  async init() {
    await this.#load();
    if (!window.firebase) return;
    firebase.initializeApp(firebaseConfig);
    this.#db = firebase.firestore();
    this.#auth = firebase.auth();
    await this.#db.enablePersistence({ synchronizeTabs: true }).catch(() => {});
    this.#auth.onAuthStateChanged((user) => this.#switchUser(user));
  }

  async #switchUser(user) {
    if (user?.uid === this.user?.uid) return;
    this.user = user;
    if (user) {
      this.#backend = new FirestoreBackend(this.#db, user.uid);
      await this.#uploadLocalData();
    } else {
      this.#backend = this.#local;
    }
    this.#attachments.clear();
    await this.#load();
  }

  // 登入前記的資料，登入時搬到雲端；全部寫入成功才清除本機
  async #uploadLocalData() {
    try {
      for (const col of COLLECTIONS) {
        const docs = await this.#local.list(col);
        await Promise.all(docs.map((doc) => this.#backend.put(col, doc.id, doc)));
        await Promise.all(docs.map((doc) => this.#local.delete(col, doc.id)));
      }
    } catch (error) {
      this.#reportError(error);
    }
  }

  async #load() {
    try {
      const [events, goals, profile] = await Promise.all(['events', 'goals', 'profile'].map((c) => this.#backend.list(c)));
      this.events = new Map(events.map((e) => [e.id, normalizeEvent(e)]));
      this.goals = goals;
      this.profile = profile[0] ?? {};
      this.#changed();
    } catch (error) {
      this.#reportError(error);
    }
  }

  reload() {
    return this.#load();
  }

  #changed() {
    this.dispatchEvent(new Event('change'));
  }

  #reportError(error) {
    console.error(error);
    this.dispatchEvent(new CustomEvent('error', { detail: error }));
  }

  // Firestore 離線時寫入要等連線才完成，所以先更新畫面，失敗再回報
  #write(col, id, doc) {
    this.#backend.put(col, id, doc).catch((error) => this.#reportError(error));
  }

  eventsOn(date) {
    return [...this.events.values()].filter((e) => e.date === date && !e.deletedAt).sort((a, b) => a.occurredAt - b.occurredAt);
  }

  liveEvents() {
    return [...this.events.values()].filter((e) => !e.deletedAt);
  }

  // 第一筆紀錄的日期；在這之前的日子不算沒做到
  firstRecordDate(eventTypes) {
    return this.liveEvents().filter((e) => eventTypes.includes(e.type)).reduce((min, e) => (!min || e.date < min ? e.date : min), null);
  }

  addEvent(type, data, { date, source = 'manual', raw = null, attachmentIds = [] } = {}) {
    if (!registry[type].validate(data)) throw new Error(`資料不完整：${type}`);
    const now = new Date();
    const today = dateKey(now);
    const eventDate = date ?? today;
    const occurredAt = eventDate === today ? now.getTime() : new Date(`${eventDate}T12:00:00`).getTime();
    const event = {
      id: crypto.randomUUID(), type, schemaVersion: registry[type].version,
      occurredAt, tzOffset: -now.getTimezoneOffset(), date: eventDate,
      source, externalId: null, data, raw, attachmentIds,
      createdAt: now.getTime(), updatedAt: now.getTime(), deletedAt: null,
    };
    this.events.set(event.id, event);
    this.#write('events', event.id, event);
    this.#changed();
    return event;
  }

  updateEvent(id, data, patch = {}) {
    const current = this.events.get(id);
    const event = { ...current, ...patch, data: { ...current.data, ...data }, updatedAt: Date.now() };
    this.events.set(id, event);
    this.#write('events', id, event);
    this.#changed();
    return event;
  }

  deleteEvent(id) {
    const event = { ...this.events.get(id), deletedAt: Date.now() };
    this.events.set(id, event);
    this.#write('events', id, event);
    this.#changed();
  }

  restoreEvent(id) {
    const event = { ...this.events.get(id), deletedAt: null };
    this.events.set(id, event);
    this.#write('events', id, event);
    this.#changed();
  }

  // 目標只增不改：同一天的修改覆蓋當天那筆，歷史判定不受影響
  saveGoal(patch) {
    const effectiveFrom = dateKey();
    const doc = { ...goalFor(this.goals, effectiveFrom), ...patch, id: effectiveFrom, effectiveFrom, updatedAt: Date.now() };
    this.goals = [...this.goals.filter((g) => g.id !== doc.id), doc];
    this.#write('goals', doc.id, doc);
    this.#changed();
  }

  saveProfile(patch) {
    this.profile = { ...this.profile, ...patch, id: 'main', updatedAt: Date.now() };
    this.#write('profile', 'main', this.profile);
    this.#changed();
  }

  async getAttachment(id) {
    if (!this.#attachments.has(id)) {
      const doc = await this.#backend.get('attachments', id);
      if (doc) this.#attachments.set(id, doc);
    }
    return this.#attachments.get(id) ?? null;
  }

  async exportAll() {
    const [events, goals, attachments, profile] = await Promise.all(COLLECTIONS.map((c) => this.#backend.list(c)));
    return { exportedAt: new Date().toISOString(), events, goals, attachments, profile };
  }

  signIn() {
    return this.#auth.signInWithPopup(new firebase.auth.GoogleAuthProvider());
  }
  signOut() {
    return this.#auth.signOut();
  }
  get canSignIn() {
    return !!this.#auth;
  }
}

export const healthStore = new HealthStore();
